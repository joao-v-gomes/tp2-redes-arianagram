#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "util.h"
#include "server.h"

// Usado para exibir as msgs de debug
// #define DEBUG

// Variáveis globais
int state;

int server_socket = 0;
int argc_counter;

// Contador para IDs de mensagens
uint32_t msg_id_counter = 0;
pthread_mutex_t msg_id_mutex = PTHREAD_MUTEX_INITIALIZER;

// Array com os clientes conectados.
// O servidor deve ser capaz de lidar com até 5 clientes simultaneamente.(por enquanto).
client_t clients[MAX_CLIENTS];

// Mutex para proteger o acesso ao array de clientes
// vou precisar iterar sobre esse array varias vezes...
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Array com as mensagens do feed.
// O feed eh circular, ou seja, quando chegar na última posição do array, a próxima mensagem deve ser inserida na primeira posição, sobrescrevendo a mensagem mais antiga.
Message feed_messages[FEED_MAX_SIZE];

//Mutex para proteger o acesso ao array de mensagens do feed
pthread_mutex_t feed_mutex = PTHREAD_MUTEX_INITIALIZER;

// Lista de pares (follower, followed) indexada por nome de usuário.
// Permite seguir usuários ainda não conectados e suporta usernames duplicados.
follow_t follow_list[MAX_FOLLOWS];
int follow_count = 0;
pthread_mutex_t follow_mutex = PTHREAD_MUTEX_INITIALIZER;


// Configura o server para Ipv4
int setServerSocketForIPv4(int *server_socket, struct sockaddr_in *server_address) {
    *server_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_address->sin_family = AF_INET;
    return OK;
}

// Configura o server para Ipv6
int setServerSocketForIPv6(int *server_socket, struct sockaddr_in6 *server_address) {
    *server_socket = socket(AF_INET6, SOCK_STREAM, 0);
    server_address->sin6_family = AF_INET6;
    return OK;
}

//Retorna o tipo do protocolo: "IPv4" ou "IPv6"
char *getProtocolType(const char *protocol) {
    if (strcmp(protocol, "v6") == 0) {
        return "IPv6";
    }
    return "IPv4";
}

// Configura o server
// criando o socket, 
// configurando o endereço e fazendo bind. 
// Retorna o socket do servidor ou ERROR em caso de falha.
int setUpServer(char *ip, int port) {

    // Validar as informações de configuração do servidor
    if (validateInfoToSetUpServer(ip,port) != 0) {
        return ERROR;
    }

    int server_socket = 0;

    // Set ipv4 or ipv6 and bind with the matching sockaddr type.
    if (strcmp(ip, "v4") == 0) {
        struct sockaddr_in server_address4;
        memset(&server_address4, 0, sizeof(server_address4));

        setServerSocketForIPv4(&server_socket, &server_address4);
        if (server_socket < 0) {
            return ERROR;
        }

        server_address4.sin_port = htons(port);

        // Seta o endereço para INADDR_ANY, 
        // que permite que o servidor aceite conexões
        // em qualquer interface de rede disponível.
        server_address4.sin_addr.s_addr = INADDR_ANY;

        // Fix o erro de "Address already in use" ao reiniciar o servidor rapidamente
        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (bind(server_socket, (struct sockaddr *)&server_address4, sizeof(server_address4)) < 0) {
            printf("Error binding server socket");
            return ERROR;
        }
    }
    else if (strcmp(ip, "v6") == 0) {
        struct sockaddr_in6 server_address6;
        memset(&server_address6, 0, sizeof(server_address6));

        setServerSocketForIPv6(&server_socket, &server_address6);
        if (server_socket < 0) {
            return ERROR;
        }

        server_address6.sin6_port = htons(port);
        server_address6.sin6_addr = in6addr_any;

        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (bind(server_socket, (struct sockaddr *)&server_address6, sizeof(server_address6)) < 0) {
            printf("Error binding server socket");
            return ERROR;
        }
    }
    else{
        return ERROR;
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        printf("Error listening for connections");
        return ERROR;
    }

#ifdef DEBUG
    printf("Servidor configurado e ouvindo por conexões...\n");
#endif
    return server_socket;
}

// Valida as informações de configuração do servidor. 
// Retorna OK se as informações forem válidas ou ERROR caso contrário.
int validateInfoToSetUpServer(char *ip, int port) {

    // Verificar se o número de argumentos é correto
    if (argc_counter != 3) {
#ifdef DEBUG
        printf("Uso: ./server <protocolo> <porta>\n");
#endif
        return ERROR;
    }

    if(strcmp(ip, "v4") != 0 && strcmp(ip, "v6") != 0) {
#ifdef DEBUG
        printf("Protocolo inválido. Use v4 ou v6.\n");
#endif
        return ERROR;
    }

    // Verificar se porta e valida
    if (port <= 0 || port > 65535) {
#ifdef DEBUG
        printf("Porta inválida. Use um valor entre 1 e 65535.\n");
#endif
        return ERROR;
    }

#ifdef DEBUG
    printf("Informações de configuração válidas. Iniciando servidor em modo %s na porta %d.\n", getProtocolType(ip), port);
#endif
    return OK;
}

// Espera pela conexao do cliente. Retorna o índice do cliente no array ou ERROR em caso de falha.
int waitForClientConnection(int server_socket) {
    int client_socket = accept(server_socket, NULL, NULL);
    if (client_socket < 0) {
        return ERROR;
    }

    // Encontrar um slot vazio no array de clientes de forma sincronizada
    pthread_mutex_lock(&clients_mutex);
    int client_index = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            client_index = i;
            break;
        }
    }
    if (client_index == -1) {
        pthread_mutex_unlock(&clients_mutex);
        // Sem slots disponíveis
        close(client_socket);
        return ERROR;
    }

    // Inicializa o slot no array ANTES de criar a thread para evitar corrida.
    clients[client_index].active = 1;
    clients[client_index].client_fd = client_socket;
    memset(clients[client_index].username, 0, USER_SIZE);
    pthread_mutex_init(&clients[client_index].send_mutex, NULL);
    pthread_mutex_unlock(&clients_mutex);

    // Criar thread passando o índice do cliente no array.
    pthread_t id;
    if (pthread_create(&id, NULL, handleClientConnection, (void *)(intptr_t)client_index) != 0) {
        pthread_mutex_lock(&clients_mutex);
        clients[client_index].active = 0;
        pthread_mutex_unlock(&clients_mutex);
        close(client_socket);
        return ERROR;
    }

    pthread_mutex_lock(&clients_mutex);
    clients[client_index].id = id;
    pthread_mutex_unlock(&clients_mutex);

    return client_index;
}

// Le uma mensagem do cliente. Retorna OK se a leitura for bem-sucedida ou ERROR em caso de falha.
int readMessageFromClient(int client_socket, Message *msg) {
    int total = 0;
    memset(msg, 0, sizeof(Message));

    while (total < sizeof(Message)) {
        int n = recv(client_socket, ((char*)msg) + total, sizeof(Message) - total, 0);

        if (n <= 0){
            return ERROR;
        }
        total += n;
    }

    messageNetworkToHost(msg);

    return OK;
}

// Função executada em thread para lidar com cada cliente conectado
void* handleClientConnection(void *arg) {

    // Detach da própria thread: ela libera seus recursos sozinha ao terminar,
    // sem precisar de pthread_join. No topo para cobrir todos os pontos de saída.
    pthread_detach(pthread_self());

    int thread_state = START;

    intptr_t client_index = (intptr_t)arg;
    
    // Acessa o array global de clientes
    // e verifica se o cliente esta ativo

    // Pega o mutex do arry global
    pthread_mutex_lock(&clients_mutex);

    if (client_index < 0 || client_index >= MAX_CLIENTS || !clients[client_index].active) {
        pthread_mutex_unlock(&clients_mutex);
        return NULL;
    }

    // Pega o socket do cliente para ler e escrever mensagens
    int client_fd = clients[client_index].client_fd;

    // Libera o acesso ao array global
    pthread_mutex_unlock(&clients_mutex);

    Message msg_received;

    // Loop principal para processar mensagens do cliente
    while (1) {

        switch (thread_state)
        {
        case START:
            memset(&msg_received, 0, sizeof(msg_received));
            thread_state = WAITING_FOR_MESSAGE;
            break;
        
        case WAITING_FOR_MESSAGE:
            if (readMessageFromClient(client_fd, &msg_received) == OK) {
                thread_state = RECEIVED_MSG;
            } else {
                // Captura o username antes de marcar o slot como inativo.
                char disc_username[USER_SIZE];
                memset(disc_username, 0, sizeof(disc_username));

                pthread_mutex_lock(&clients_mutex);
                if (client_index >= 0 && client_index < MAX_CLIENTS) {
                    strncpy(disc_username, clients[client_index].username, USER_SIZE - 1);
                    disc_username[USER_SIZE - 1] = '\0';
                    clients[client_index].active = 0;
                }
                pthread_mutex_unlock(&clients_mutex);

                // Só loga [DISC] se o cliente chegou a registrar um username.
                if (disc_username[0] != '\0') {
                    printf("[DISC] %s desconectou.\n", disc_username);
                }

                close(client_fd);
                return NULL;
            }
            break;

        case RECEIVED_MSG: {
            // Cliente precisa registrar username via MSG_CONNECT antes de outras mensagens.
            bool username_registered = false;
            char stored_username[USER_SIZE];
            memset(stored_username, 0, sizeof(stored_username));

            pthread_mutex_lock(&clients_mutex);
            if (client_index >= 0 && client_index < MAX_CLIENTS && clients[client_index].active) {
                if (clients[client_index].username[0] != '\0') {
                    username_registered = true;
                    strncpy(stored_username, clients[client_index].username, USER_SIZE - 1);
                    stored_username[USER_SIZE - 1] = '\0';
                }
            }
            pthread_mutex_unlock(&clients_mutex);

            if (!username_registered && msg_received.type != MSG_CONNECT) {
#ifdef DEBUG
                printf("Cliente %d enviou mensagem sem registrar username. Encerrando conexão.\n", (int)client_index);
#endif
                pthread_mutex_lock(&clients_mutex);
                if (client_index >= 0 && client_index < MAX_CLIENTS) {
                    clients[client_index].active = 0;
                }
                pthread_mutex_unlock(&clients_mutex);
                close(client_fd);
                return NULL;
            }

            if (username_registered && msg_received.type != MSG_CONNECT) {
                strncpy(msg_received.username, stored_username, USER_SIZE - 1);
                msg_received.username[USER_SIZE - 1] = '\0';
            }

            switch (msg_received.type) {
            case MSG_CONNECT:
                thread_state = RECEIVED_MSG_CONNECT;
                break;

            case MSG_POST:
                thread_state = RECEIVED_MSG_POST;
                break;

            case MSG_FOLLOW:
                thread_state = RECEIVED_MSG_FOLLOW;
                break;

            case MSG_READ:
                thread_state = RECEIVED_MSG_READ;
                break;

            default:
#ifdef DEBUG
                printf("Recebida mensagem de tipo desconhecido do cliente %s\n", msg_received.username);
#endif
                break;
            }

            break;
        }

        case RECEIVED_MSG_CONNECT:
            msg_received.username[USER_SIZE - 1] = '\0';
            if (msg_received.username[0] == '\0') {
#ifdef DEBUG
                printf("MSG_CONNECT inválida: username vazio (slot %d). Encerrando conexão.\n", (int)client_index);
#endif
                pthread_mutex_lock(&clients_mutex);
                if (client_index >= 0 && client_index < MAX_CLIENTS) {
                    clients[client_index].active = 0;
                }
                pthread_mutex_unlock(&clients_mutex);
                close(client_fd);
                return NULL;
            }

            pthread_mutex_lock(&clients_mutex);
            if (client_index >= 0 && client_index < MAX_CLIENTS && clients[client_index].active) {
                if (clients[client_index].username[0] == '\0') {
                    strncpy(clients[client_index].username, msg_received.username, USER_SIZE - 1);
                    clients[client_index].username[USER_SIZE - 1] = '\0';
                }
                strncpy(msg_received.username, clients[client_index].username, USER_SIZE - 1);
                msg_received.username[USER_SIZE - 1] = '\0';
            }
            pthread_mutex_unlock(&clients_mutex);

            printf("[CONN] %s conectou.\n", msg_received.username);
            thread_state = WAITING_FOR_MESSAGE;
            break;

        case RECEIVED_MSG_POST:
            processPostMessage(&msg_received);
            thread_state = WAITING_FOR_MESSAGE;
            break;

        case RECEIVED_MSG_FOLLOW:
            processFollowMessage(&msg_received);
            thread_state = WAITING_FOR_MESSAGE;
            break;

        case RECEIVED_MSG_READ:
            processReadMessage(&msg_received, (int)client_index);
            thread_state = WAITING_FOR_MESSAGE;
            break;

        default:
            break;
        }
    }
}

// Incrementa e retorna o novo ID
uint32_t incrementMsgIdCounter() {
    pthread_mutex_lock(&msg_id_mutex);
    uint32_t new_id = ++msg_id_counter;
    pthread_mutex_unlock(&msg_id_mutex);
    return new_id;
}

// Preenche o feed com a nova mensagem, removendo a mensagem mais antiga se o feed estiver cheio.
void addMsgToFeed(Message *msg) {
    pthread_mutex_lock(&feed_mutex);
    // Shift das mensagens para a direita para abrir espaço para a nova mensagem no início do array
    for (int i = FEED_MAX_SIZE - 1; i > 0; i--) {
        feed_messages[i] = feed_messages[i - 1];
    }
    feed_messages[0] = *msg;
    pthread_mutex_unlock(&feed_mutex);
}

// Envia as mensagens do feed para o cliente (mais nova → mais antiga) seguidas de MSG_END.
// Todo o envio é protegido por send_mutex para não intercalar com pushes assíncronos.
void sendFeedToClient(int client_index) {
    Message feed_to_send[FEED_MAX_SIZE];
    int feed_count = getFeedMessages(feed_to_send);

    pthread_mutex_lock(&clients[client_index].send_mutex);

    for (int i = 0; i < feed_count; i++) {
        Message msg_to_send = feed_to_send[i];
        msg_to_send.type = MSG_PUSH;
        messageHostToNetwork(&msg_to_send);
        send(clients[client_index].client_fd, &msg_to_send, sizeof(Message), 0);
    }

    // MSG_END obrigatório ao final, independentemente de quantas mensagens foram enviadas.
    Message end_msg;
    memset(&end_msg, 0, sizeof(Message));
    end_msg.type = MSG_END;
    messageHostToNetwork(&end_msg);
    send(clients[client_index].client_fd, &end_msg, sizeof(Message), 0);

    pthread_mutex_unlock(&clients[client_index].send_mutex);
}

int getFeedMessages(Message *feed) {
    int feed_count = 0;

    pthread_mutex_lock(&feed_mutex);
    for (int i = 0; i < FEED_MAX_SIZE; i++) {
        if (feed_messages[i].msg_id != 0) {
            feed[feed_count] = feed_messages[i];
            feed_count++;
        }
    }
    pthread_mutex_unlock(&feed_mutex);

    return feed_count;
}

bool processPostMessage(Message *msg) {
    msg->msg_id = incrementMsgIdCounter();

    addMsgToFeed(msg);

    printf("[LOG] %s posted (ID %u): \"%s\"\n", msg->username, msg->msg_id, msg->content);

    // Passo 1: coletar os nomes dos seguidores do poster sem segurar o lock por muito tempo.
    char to_notify[MAX_CLIENTS][USER_SIZE];
    int n_notify = 0;

    pthread_mutex_lock(&follow_mutex);
    for (int i = 0; i < follow_count && n_notify < MAX_CLIENTS; i++) {
        if (strcmp(follow_list[i].followed, msg->username) == 0) {
            strncpy(to_notify[n_notify], follow_list[i].follower, USER_SIZE - 1);
            to_notify[n_notify][USER_SIZE - 1] = '\0';
            n_notify++;
        }
    }
    pthread_mutex_unlock(&follow_mutex);

    // Passo 2: para cada seguidor, encontrar os slots ativos com aquele username.
    for (int f = 0; f < n_notify; f++) {
        int slots[MAX_CLIENTS];
        int n_slots = 0;

        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active && strcmp(clients[i].username, to_notify[f]) == 0) {
                slots[n_slots++] = i;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        // Passo 3: enviar MSG_PUSH para cada socket, protegido pelo send_mutex do slot.
        for (int k = 0; k < n_slots; k++) {
            int idx = slots[k];
            Message push_msg = *msg;
            push_msg.type = MSG_PUSH;
            messageHostToNetwork(&push_msg);

            pthread_mutex_lock(&clients[idx].send_mutex);
            // Verifica active e username de novo após adquirir o lock, pois o slot pode ter
            // sido reusado entre a coleta e o envio.
            if (clients[idx].active && strcmp(clients[idx].username, to_notify[f]) == 0) {
                send(clients[idx].client_fd, &push_msg, sizeof(Message), 0);
            }
            pthread_mutex_unlock(&clients[idx].send_mutex);
        }
    }

    return true;
}

bool processFollowMessage(Message *msg) {
    // Auto-follow
    // Verifica se o usuário está tentando seguir a si mesmo. Se sim, ignora silenciosamente.
    if (strcmp(msg->username, msg->content) == 0) {
        return true;
    }

    //Pega o muter de follow
    pthread_mutex_lock(&follow_mutex);

    // Follow duplicado
    // Verifica se o par (follower, followed) já existe. Se sim, ignora silenciosamente.
    for (int i = 0; i < follow_count; i++) {
        if (strcmp(follow_list[i].follower, msg->username) == 0 &&
            strcmp(follow_list[i].followed, msg->content) == 0) {
            pthread_mutex_unlock(&follow_mutex);
            return true;
        }
    }

    // Registra o novo follow (o followed pode não estar conectado ainda).
    if (follow_count < MAX_FOLLOWS) {
        strncpy(follow_list[follow_count].follower, msg->username, USER_SIZE - 1);
        follow_list[follow_count].follower[USER_SIZE - 1] = '\0';
        strncpy(follow_list[follow_count].followed, msg->content, USER_SIZE - 1);
        follow_list[follow_count].followed[USER_SIZE - 1] = '\0';
        follow_count++;
    }

    pthread_mutex_unlock(&follow_mutex);
    return true;
}

bool processReadMessage(Message *msg, int client_index) {
    if (client_index >= 0 && client_index < MAX_CLIENTS) {
        sendFeedToClient(client_index);
    }
    return true;
}

int main(int argc, char **argv) {

    // Inicializa o estado da FSM
    state = START_SERVER_STATE;

    argc_counter = argc;

    while (1)
    {
        switch (state)
        {
            case START_SERVER_STATE:
                state = SETTING_UP_SERVER_STATE;
                #ifdef DEBUG
                printf("Iniciando servidor...\n");
                #endif
                break;
            
            //Precisei colocar o case dentro de chaves para declarar variáveis locais
            case SETTING_UP_SERVER_STATE: {
                char *ip_type = argv[1];
                int port = atoi(argv[2]);

                server_socket = setUpServer(ip_type, port);

                if (server_socket < 0) {
                    printf("Erro ao configurar o servidor\n");
                    return ERROR;
                }
                else{
                    printf("Aguardando conexoes na porta %d.\n", port);
                    state = WAIT_FOR_CONNECTION_STATE;
                }

                break;
            }

            case WAIT_FOR_CONNECTION_STATE: {
                int client_index = waitForClientConnection(server_socket);

                if (client_index < 0) {
#ifdef DEBUG
                    printf("Erro ao aceitar conexão do cliente\n");
#endif
                }
#ifdef DEBUG
                else {
                    printf("Cliente conectado (índice %d)\n", client_index);
                }
#endif

                break;
            }

            case EXIT_STATE:
                exit(0);
                break;

            default:
                break;
        }
    }
    return 0;
}