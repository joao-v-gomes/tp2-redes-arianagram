#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "util.h"
#include "server.h"
#include "ifaddrs.h"

// Usado para exibir as msgs de debug
#define DEBUG

// Variáveis globais
int state;

int server_socket = 0;
int argc_counter;
int code = 0;

// Contador para IDs de mensagens
uint32_t msg_id_counter = 0;
pthread_mutex_t msg_id_mutex = PTHREAD_MUTEX_INITIALIZER;

// Array com os clientes conectados. O servidor deve ser capaz de lidar com até 5 clientes simultaneamente.(por enquanto)
client_t clients[MAX_CLIENTS];

// Mutex para proteger o acesso ao array de clientes
// vou precisar iterar sobre esse array varias vezes...
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;



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

    if (listen(server_socket, 1) < 0) {
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
    pthread_mutex_unlock(&clients_mutex);

    if (client_index == -1) {
        // Sem slots disponíveis
        close(client_socket);
        return ERROR;
    }

    // Inicializar a estrutura do cliente
    client_t new_client;
    new_client.active = 1;
    new_client.client_fd = client_socket;
    memset(new_client.username, 0, USER_SIZE);
    new_client.queue_head = NULL;
    new_client.queue_tail = NULL;
    pthread_mutex_init(&new_client.queue_mutex, NULL);
    pthread_cond_init(&new_client.queue_cond, NULL);

    // Criar thread passando o ÍNDICE do cliente no array
    pthread_t id;
    pthread_create(&id, NULL, handleClientConnection, (void *)(intptr_t)client_index);
    new_client.id = id;

    // Adicionar ao array de forma sincronizada
    pthread_mutex_lock(&clients_mutex);
    clients[client_index] = new_client;
    pthread_mutex_unlock(&clients_mutex);

#ifdef DEBUG
    printf("Cliente conectado (índice %d)\n", client_index);
#endif
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

// Envia uma mensagem para o cliente
int sendMessageToClient(int client_socket, Message *msg) {
    messageHostToNetwork(msg);
    int n = send(client_socket, msg, sizeof(Message), 0);
    if (n <= 0) {
        return ERROR;
    }
    else{
        return OK;
    }
}

// Função executada em thread para lidar com cada cliente conectado
void* handleClientConnection(void *arg) {

    int thread_state = START;

    intptr_t client_index = (intptr_t)arg;
    
    // Acessa o array global de clientes
    // e verifica se o cliente esta ativo

    // Pega o mutex do arry global
    pthread_mutex_lock(&clients_mutex);

    if (client_index < 0 || client_index >= MAX_CLIENTS){
        if(!clients[client_index].active){
            // Libera o acesso ao array global
            // e retorna NULL
            pthread_mutex_unlock(&clients_mutex);
            return NULL;
        }
    } 

    // Pega o socket do cliente para ler e escrever mensagens
    int client_fd = clients[client_index].client_fd;

    // Libera o acesso ao array global
    pthread_mutex_unlock(&clients_mutex);

    Message msg_received, msg_to_send;
    
    // Loop principal para processar mensagens do cliente
    while (1) {

        switch (thread_state)
        {
        case START:
            memset(&msg_received, 0, sizeof(msg_received));
            thread_state = WAITING_FOR_MESSAGE;
            break;
        
        case WAITING_FOR_MESSAGE:
            if (readMessageFromClient(client_fd, &msg_received) != OK) {
                thread_state = RECEIVED_MSG;

            }
            break;

        case RECEIVED_MSG:
            switch (msg_received.type)
            {
            case MSG_POST:
                printf("Recebida mensagem de POST do cliente %s: %s\n", msg_received.username, msg_received.content);
                break;
            
            case MSG_FOLLOW:
                printf("Recebida mensagem de FOLLOW do cliente %s: %s\n", msg_received.username, msg_received.content);
                break;

            case MSG_READ:
                printf("Recebida mensagem de READ do cliente %s\n", msg_received.username);
                break;

            default:
                printf("Recebida mensagem de tipo desconhecido do cliente %s\n", msg_received.username);
                break;
            }

        case RECEIVED_MSG_POST:
            printf("Processando mensagem de POST do cliente %s: %s\n", msg_received.username, msg_received.content);
            thread_state = WAITING_FOR_MESSAGE;
            break;
        
        case RECEIVED_MSG_FOLLOW:
            printf("Processando mensagem de FOLLOW do cliente %s: %s\n", msg_received.username, msg_received.content);
            thread_state = WAITING_FOR_MESSAGE;
            break;
        
        case RECEIVED_MSG_READ:
            printf("Processando mensagem de READ do cliente %s\n", msg_received.username);
            thread_state = WAITING_FOR_MESSAGE;
            break;

        case SEND_MSG_TO_CLIENT:
            printf("Enviando mensagem de resposta para o cliente %s\n", msg_received.username);
            thread_state = WAITING_FOR_MESSAGE;
            break;

        default:
            break;
        }
    }
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
                    printf("Aguardando conexões na porta %d\n", port);
                    state = WAIT_FOR_CONNECTION_STATE;
                }

                break;
            }

            case WAIT_FOR_CONNECTION_STATE: {
                int client_index = waitForClientConnection(server_socket);
                
                if (client_index < 0) {
                    printf("Erro ao aceitar conexão do cliente\n");
                }
                else{
                    printf("Cliente conectado (índice %d)\n", client_index);
                    // A thread está rodando agora, voltamos a esperar por mais conexões
                }

                break;
            }
            // case WAIT_FOR_START_MESSAGE_STATE:
            //     memset(&msg_received, 0, sizeof(msg_received));

            //     if (readMessageFromClient(client_socket, &msg_received) == ERROR) {
            //         return ERROR;
            //     }

            //     if(msg_received.type == MSG_START) {
            //         state = RECEIVED_START_MESSAGE_STATE;
            //     }
            //     else{
            //         return ERROR;
            //     }

            //     break;

            // case RECEIVED_START_MESSAGE_STATE:
            //     state = WAITING_FOR_MESSAGE_STATE;
            //     attempts_counter = 1;

            //     // Ao receber a msg de START, limpa as msg para uiniciar do zero.
            //     // Seta o attempts_counter para 1.
            //     memset(&msg_received, 0, sizeof(msg_received));
            //     memset(&msg_to_send, 0, sizeof(msg_to_send));

            //     break;    

            // case WAITING_FOR_MESSAGE_STATE:

            //     // Aguarda o palpite do cliente.
            //     memset(&msg_received, 0, sizeof(msg_received));

            //     if (readMessageFromClient(client_socket, &msg_received) == ERROR) {
            //         return ERROR;
            //     }
            //     else if (msg_received.type == MSG_GUESS) {
            //         state = RECEIVED_GUESS_MESSAGE_STATE;
            //     }
            //     else{
            //         return ERROR;
            //     }

            //     break;
            
            // case RECEIVED_GUESS_MESSAGE_STATE: {
            //     state = SEND_FEEDBACK_STATE;

            //     break;
            // }

            // case SEND_FEEDBACK_STATE:
            //     memset(&msg_to_send, 0, sizeof(msg_to_send));

            //     if (!isValidGuess(msg_received.guess)) {
            //         msg_to_send.type = MSG_ERROR;
            //         msg_to_send.win_status = ERROR;
            //         msg_to_send.attempts = attempts_counter;
            //         setMessage(&msg_to_send, "Insira uma sequência válida!");

            //         messageHostToNetwork(&msg_to_send);

            //         send(client_socket, &msg_to_send, sizeof(msg_to_send), 0);

            //         messageNetworkToHost(&msg_to_send);

            //         state = WAITING_FOR_MESSAGE_STATE;
            //         break;
            //     }

            //     fillFeedbackWithGuess(msg_received.guess, &msg_to_send);

            //     calculateFeedback(msg_received.guess, code, &msg_to_send);

            //     // Incrementa as tentativas se elas forem validas
            //     msg_to_send.attempts = attempts_counter++;

            //     if (msg_to_send.win_status == IN_GAME) {
            //         msg_to_send.type = MSG_FEEDBACK;
            //         char feedback[6];
            //         buildFeedbackString(&msg_to_send, feedback);
            //         snprintf(msg_to_send.message, MSG_SIZE,
            //                  "Dica: %s\nTentativas realizadas: %d",
            //                  feedback,
            //                  msg_to_send.attempts);
            //     }
            //     else if (msg_to_send.win_status == WIN) {
            //         msg_to_send.type = MSG_WIN;
            //         setMessage(&msg_to_send, "Acesso concedido! Thaísa recuperou o sistema!");
            //     }
            //     else {
            //         return ERROR;
            //     }

            //     messageHostToNetwork(&msg_to_send);

            //     send(client_socket, &msg_to_send, sizeof(msg_to_send), 0);

            //     messageNetworkToHost(&msg_to_send);

            //     // Verifica o feedback para decidir o próximo estado. 
            //     // Se o cliente ganhou, espera a mensagem de exit. 
            //     //Caso contrário, espera um novo palpite.
            //     if(msg_to_send.win_status == IN_GAME) {
            //         state = WAITING_FOR_MESSAGE_STATE;
            //     }
            //     else if (msg_to_send.win_status == WIN) {
            //         state = WAIT_FOR_EXIT_MESSAGE_STATE;
            //     }
            //     else{
            //         return ERROR;
            //     }

            //     break;

            // case WAIT_FOR_EXIT_MESSAGE_STATE:
            //     memset(&msg_received, 0, sizeof(msg_received));
            //     if(readMessageFromClient(client_socket, &msg_received) == ERROR) {
            //         return ERROR;
            //     }

            //     if (msg_received.type == MSG_EXIT) {
            //         state = EXIT_STATE;
            //         memset(&msg_to_send, 0, sizeof(msg_to_send));
            //         setMessage(&msg_to_send, "Cliente desconectado");
            //         printf("%s\n", msg_to_send.message);

            //         // Fecha os sockets do cliente e do servidor antes de sair.
            //         close(client_socket);
            //         close(server_socket);

            //     }
            //     else{
            //         return ERROR;
            //     }

            //     break;

            case EXIT_STATE:
                exit(0);
                break;

            default:
                break;
        }
    }
    return 0;
}