#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>
#include <stdbool.h>
#include "util.h"
#include "client.h"

// #define DEBUG

// Conta os argumentos para validar a quantidade de argumentos passados na linha de comando
int argc_counter;

// Variáveis globais para o estado da FSM e o socket do cliente
int state;
int client_socket;
char user_input[256];
char username[USER_SIZE];

// Flag compartilhada entre a thread principal e a leitora:
// 1 enquanto o cliente aguarda a resposta de um MSG_READ (itens do feed),
// 0 fora desse contexto (notificações ao vivo).
volatile int in_feed = 0;

// Estruturas para armazenar as mensagens enviadas e recebidas. Uma de cada vez...
Message msg_sent;
Message msg_to_send;
Message msg_received;

// Se conecta ao servidor usando o IP e a porta fornecidos. Retorna o socket do cliente ou ERROR em caso de falha.
// Testa a conexao IpV4 primeiro, se falhar tenta o IPv6. Se ambos falharem, retorna ERROR.
int connectToServer(char *server_ip, int server_port, char *user_name) {
    if(validateInfoToConnectToServer(server_ip, server_port, user_name) != 0) {
        return ERROR;
    }

    int client_socket = ERROR;

    struct sockaddr_in server_address_v4;
    memset(&server_address_v4, 0, sizeof(server_address_v4));
    server_address_v4.sin_family = AF_INET;
    server_address_v4.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_address_v4.sin_addr) == 1) {
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket < 0) {
            return ERROR;
        }

        if (connect(client_socket, (struct sockaddr *)&server_address_v4, sizeof(server_address_v4)) < 0) {
            close(client_socket);
            return ERROR;
        }

        setUsername(user_name);
        return client_socket;
    }

    struct sockaddr_in6 server_address_v6;
    memset(&server_address_v6, 0, sizeof(server_address_v6));
    server_address_v6.sin6_family = AF_INET6;
    server_address_v6.sin6_port = htons(server_port);

    if (inet_pton(AF_INET6, server_ip, &server_address_v6.sin6_addr) == 1) {
        client_socket = socket(AF_INET6, SOCK_STREAM, 0);
        if (client_socket < 0) {
            return ERROR;
        }

        if (connect(client_socket, (struct sockaddr *)&server_address_v6, sizeof(server_address_v6)) < 0) {
            close(client_socket);
            return ERROR;
        }
#ifdef DEBUG
        printf("Conectado ao servidor %s na porta %d usando IPv6.\n", server_ip, server_port);
#endif  
        setUsername(user_name);
        return client_socket;
    }

    return ERROR;
}

// Valida as informações de configuração do cliente. Retorna OK se as informações forem válidas ou ERROR caso contrário.
int validateInfoToConnectToServer(char *server_ip, int server_port, char *username) {

    // Verificar se o número de argumentos é correto
    if (argc_counter != 4) {
#ifdef DEBUG
        printf("Uso: ./client <endereço ip> <porta> <username>\n");
#endif
        return ERROR;
    }

    // Verificar se porta e valida
    if (server_port <= 0 || server_port > 65535) {
#ifdef DEBUG
        printf("Porta inválida. Use um valor entre 1 e 65535.\n");
#endif
        return ERROR;
    }

    // Verificar se o IP do servidor é válido (IPv4 ou IPv6 literal)
    struct in_addr sa_v4;
    struct in6_addr sa_v6;

    if (inet_pton(AF_INET, server_ip, &sa_v4) != 1 && inet_pton(AF_INET6, server_ip, &sa_v6) != 1) {
#ifdef DEBUG
        printf("Endereço IP inválido.\n");
#endif
        return ERROR;
    }

    // Verificar se o username é válido (não vazio e sem espaços)
    if (strlen(username) == 0 || strchr(username, ' ') != NULL || strlen(username) >= USER_SIZE) {
#ifdef DEBUG
        printf("Username inválido.\n");
#endif
        return ERROR;
    }

#ifdef DEBUG
    printf("Informações de configuração válidas. Tentando conectar ao servidor %s na porta %d com username '%s'.\n", server_ip, server_port, username);
#endif
    return OK;
}

// Lê uma mensagem do servidor. Retorna OK se a leitura for bem-sucedida ou ERROR em caso de falha.
int readMessageFromServer(int client_socket, Message *msg) {

#ifdef DEBUG
    printf("Lendo mensagem do servidor...\n");
#endif

    int total = 0;
    memset(msg, 0, sizeof(Message));

    while (total < sizeof(Message)) {
        int n = recv(client_socket, ((char*)msg) + total, sizeof(Message) - total, 0);

        // printf("Read %d bytes from server socket\n", n);

        if (n <= 0){
            return ERROR;
        }
        total += n;
    }

    messageNetworkToHost(msg);

    return OK;
}

int sendMessageToServer(int client_socket, Message *msg) {
    Message msg_to_send = *msg;
    messageHostToNetwork(&msg_to_send);

    int total = 0;
    while (total < sizeof(Message)) {
        int n = send(client_socket, ((char *)&msg_to_send) + total, sizeof(Message) - total, 0);
        if (n <= 0) {
            return ERROR;
        }
        total += n;
    }

    return OK;
}


bool validateUserInput(char *input) {
    // Aqui você pode implementar a validação da entrada do usuário
    // Por exemplo, verificar se o comando é "post", "follow" ou "read" e se os argumentos estão corretos
    // Retorna true se a entrada for válida ou false caso contrário
    char *input_temp = strdup(input); // Cria uma cópia da entrada para tokenização

    // verifica se a primeira palavra é um comando válido: POST, FOLLOW ou READ
    char *command = strtok(input_temp, " ");
    if (command == NULL) {
        free(input_temp);
        return false;
    }

    if (strcmp(command, "POST") != 0 && strcmp(command, "FOLLOW") != 0 && strcmp(command, "READ") != 0) {
        free(input_temp);
        return false;
    }

    // Verifica se o comando é POST e se tem um conteúdo válido
    if (strcmp(command, "POST") == 0) {
        char *content = strtok(NULL, "");
        if (content == NULL || strlen(content) == 0 || strlen(content) >= CONTENT_SIZE) {
            free(input_temp);
            return false;
        }
        // Guarda só o conteúdo (sem o "POST ") no campo content.
        // Copia antes do free(input_temp), pois content aponta para dentro dele.
        strncpy(msg_to_send.content, content, CONTENT_SIZE - 1);
        msg_to_send.content[CONTENT_SIZE - 1] = '\0';
        msg_to_send.type = MSG_POST;
    }

    // Verifica se o comando é FOLLOW e se tem um username válido
    if (strcmp(command, "FOLLOW") == 0) {
        char *target_username = strtok(NULL, " ");
        if (target_username == NULL || strlen(target_username) == 0 || strlen(target_username) >= USER_SIZE) {
            free(input_temp);
            return false;
        }
        // Guarda só o alvo do follow (ex: "@ariana") no campo content.
        strncpy(msg_to_send.content, target_username, USER_SIZE - 1);
        msg_to_send.content[USER_SIZE - 1] = '\0';
        msg_to_send.type = MSG_FOLLOW;
    }

    // Verifica se o comando é READ e se não tem argumentos extras
    if (strcmp(command, "READ") == 0) {
        char *extra = strtok(NULL, " ");
        if (extra != NULL) {
            free(input_temp);
            return false;
        }
        msg_to_send.type = MSG_READ;
    }

#ifdef DEBUG
    printf("Entrada do usuário válida: %s\n", input);
#endif
    free(input_temp);
    return true;
}

char *getUsername() {
    return username;
}

void setUsername(char *new_username) {
    strncpy(username, new_username, USER_SIZE - 1);
    username[USER_SIZE - 1] = '\0'; // Garantir terminação nula
}

void *readFromServer(void *socket) {
    int client_socket = *(int *)socket;
    Message msg;

#ifdef DEBUG
    printf("Thread de leitura do servidor iniciada.\n");
#endif

    while (1) {
        if (readMessageFromServer(client_socket, &msg) == ERROR) {
#ifdef DEBUG
            printf("Erro ao ler mensagem do servidor. Conexão pode ter sido encerrada.\n");
#endif
            break;
        }

        switch (msg.type) {
            case MSG_PUSH:
                if (in_feed) {
                    printf("[FEED] ID %u | %s: \"%s\"\n", msg.msg_id, msg.username, msg.content);
                } else {
                    printf("[NOTIFICATION] %s: \"%s\"\n", msg.username, msg.content);
                }
                break;
            case MSG_END:
                // Fim silencioso do feed: reseta o flag.
                in_feed = 0;
                break;
            default:
#ifdef DEBUG
                printf("Recebida mensagem de tipo desconhecido do servidor:\n");
                printMsg(&msg);
#endif
                break;
        }
    }

    return NULL;
}

int main(int argc, char **argv) {

    argc_counter = argc;

    // Inicializa o estado da FSM
    state = START_CLIENT_STATE;

    while(1)
    {
        switch (state)
        {
            case START_CLIENT_STATE:
                state = CONNECT_TO_SERVER_STATE;
                break;
            
            case CONNECT_TO_SERVER_STATE: {
                char *server_ip = argv[1];
                int server_port = atoi(argv[2]);
                char *username = argv[3];

                client_socket = connectToServer(server_ip, server_port, username);

                if (client_socket < 0) {
                    return ERROR;
                }
                else{
                    printf("Conectado ao servidor como %s.\n", username);
                    pthread_t read_thread;
                    pthread_create(&read_thread, NULL, readFromServer, &client_socket);
                    pthread_detach(read_thread);
                    state = SEND_CONNECT_MESSAGE_STATE;
                    break;
                }
            }

            //Envia o MSG_CONNECT para o servidor,
            // para que ele possa registrar o username do cliente e associar ao socket.
            case SEND_CONNECT_MESSAGE_STATE: {
                
                Message connect_msg;
                memset(&connect_msg, 0, sizeof(connect_msg));

                connect_msg.type = MSG_CONNECT;
                strncpy(connect_msg.username, getUsername(), USER_SIZE - 1);
                connect_msg.username[USER_SIZE - 1] = '\0'; // Garantir terminação nula

                if (sendMessageToServer(client_socket, &connect_msg) == ERROR) {
                    return ERROR;
                }

                state = WAIT_USER_INPUT_STATE;
                break;
            }
            
            case WAIT_USER_INPUT_STATE: {

                //Limpa o user_input para evitar lixo de memória
                memset(user_input, 0, sizeof(user_input));

                // Solicita a entrada do usuário
                if (fgets(user_input, sizeof(user_input), stdin) == NULL) {
                    printf("Entrada encerrada. Desconectando cliente.\n");
                    close(client_socket);
                    return ERROR;
                }

                // Remove o caractere de nova linha, se presente
                user_input[strcspn(user_input, "\n")] = 0;
                if (strlen(user_input) == 0) {
                    printf("Entrada vazia. Tente novamente.\n");
                    state = WAIT_USER_INPUT_STATE;
                    break;
                }
                else {
                    state = VALIDATE_USER_INPUT_STATE;
                    break;
                }

            }

            case VALIDATE_USER_INPUT_STATE: {
                // Limpa a mensagem antes de montá-la para não sobrar lixo de um comando
                // anterior (ex: content de um POST antigo aparecendo num READ).
                memset(&msg_to_send, 0, sizeof(msg_to_send));

                // validateUserInput define msg_to_send.type e já preenche msg_to_send.content
                // com só o conteúdo do POST / só o alvo do FOLLOW (sem o nome do comando).
                if (!validateUserInput(user_input)) {
                    printf("Entrada inválida. Tente novamente.\n");
                    state = WAIT_USER_INPUT_STATE;
                    break;
                }
                else {
                    strcpy(msg_to_send.username, getUsername());
                    state = SEND_MSG_TO_SERVER_STATE;
                    break;
                }
            }

            case SEND_MSG_TO_SERVER_STATE: {
#ifdef DEBUG
                printMsg(&msg_to_send);
#endif
                // Ativa o flag antes de enviar para que a thread leitora já esteja
                // pronta para imprimir [FEED] quando os MSG_PUSH chegarem.
                if (msg_to_send.type == MSG_READ) {
                    in_feed = 1;
                }

                if (sendMessageToServer(client_socket, &msg_to_send) == ERROR) {
                    return ERROR;
                }

                state = WAIT_USER_INPUT_STATE;
                break;
            }

            case EXIT_STATE:
                exit(0);
                break;

            default:
                break;
        }
    }
}