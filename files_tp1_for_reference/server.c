#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
// #include "util.h"
#include "server.h"
#include "ifaddrs.h"

// Usado para exibir as msgs de debug
// #define DEBUG

// Variáveis globais para o 
// estado da FSM, 
// os sockets do servidor e do cliente, 
// o código a ser adivinhado
// e o contador de tentativas
int state;

int argc_counter;

int server_socket = 0;
int client_socket = 0;
int code;
int attempts_counter = 0;

// Estruturas para armazenar as mensagens enviadas e recebidas. Uma de cada vez...
HackerMessage msg_received;
HackerMessage msg_to_send;

// Trata o problema da endianess do Host/Rede,
// convertendo os campos da mensagem de host para network byte order antes de enviar,
// e de network para host byte order após receber.
void messageHostToNetwork(HackerMessage *msg) {
    msg->type = htonl(msg->type);
    for (int i = 0; i < 5; i++) {
        msg->guess[i] = htonl(msg->guess[i]);
        msg->feedback[i] = htonl(msg->feedback[i]);
    }
    msg->attempts = htonl(msg->attempts);
    msg->win_status = htonl(msg->win_status);
}

void messageNetworkToHost(HackerMessage *msg) {
    msg->type = ntohl(msg->type);
    for (int i = 0; i < 5; i++) {
        msg->guess[i] = ntohl(msg->guess[i]);
        msg->feedback[i] = ntohl(msg->feedback[i]);
    }
    msg->attempts = ntohl(msg->attempts);
    msg->win_status = ntohl(msg->win_status);
}

// Retorna o tipo do protocolo: "IPv4" ou "IPv6"
char *getProtocolType(const char *protocol) {
    if (strcmp(protocol, "v6") == 0) {
        return "IPv6";
    }
    return "IPv4";
}

// Configura a mensagem de resposta do servidor com o texto fornecido.
void setMessage(HackerMessage *msg, const char *text) {
    snprintf(msg->message, MSG_SIZE, "%s", text);
}

// Constrói a string de feedback para o cliente com base na mensagem recebida do cliente.
// Usa os caracteres '_' para indicar dígitos que não estão no código,
// '*' para dígitos que estão no código mas na posição errada, e os próprios dígitos para os acertos.
void buildFeedbackString(const HackerMessage *msg, char *feedback) {
    for (int i = 0; i < 5; i++) {
        if (msg->feedback[i] == RIGHT_POSITION) {
            feedback[i] = msg->guess[i] + '0';
        }
        else if (msg->feedback[i] == WRONG_POSITION) {
            feedback[i] = '*';
        }
        else {
            feedback[i] = '_';
        }
    }

    feedback[5] = '\0';
}

// Verifica se o palpite é válido. 
// O palpite deve conter exatamente 5 dígitos, cada um entre 0 e 9.
int isValidGuess(const int *guess) {
    for (int i = 0; i < 5; i++) {
        if (guess[i] < 0 || guess[i] > 9) {
            return 0;
        }
    }

    return 1;
}

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

// Configura o server
// criando o socket, 
// configurando o endereço e fazendo bind. 
// Retorna o socket do servidor ou ERROR em caso de falha.
int setUpServer(char *ip, int port, int code) {

    // Validar as informações de configuração do servidor
    if (validateInfoToSetUpServer(ip,port,code) != 0) {
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

    return server_socket;
}

// Valida as informações de configuração do servidor. 
// Retorna OK se as informações forem válidas ou ERROR caso contrário.
int validateInfoToSetUpServer(char *ip, int port, int code) {

    // Verificar se o número de argumentos é correto
    if (argc_counter != 4) {
#ifdef DEBUG
        printf("Uso: ./server <protocolo> <porta> <senha>\n");
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

    if(code < 0 || code > 99999) {
#ifdef DEBUG
        printf("Senha inválida. Use um valor entre 00000 e 99999.\n");
#endif
        return ERROR;
    }

    return OK;
}

// Espera pela conexao do cliente. Retorna o socket do cliente ou ERROR em caso de falha.
int waitForClientConnection(int server_socket) {
    int client_socket = accept(server_socket, NULL, NULL);
    if (client_socket < 0) {
        return ERROR;
    }
    return client_socket;
}

// Le uma mensagem do cliente. Retorna OK se a leitura for bem-sucedida ou ERROR em caso de falha.
int readMessageFromClient(int client_socket, HackerMessage *msg) {
    int total = 0;
    memset(msg, 0, sizeof(HackerMessage));

    while (total < sizeof(HackerMessage)) {
        int n = recv(client_socket, ((char*)msg) + total, sizeof(HackerMessage) - total, 0);

        // printf("Read %d bytes from client socket\n", n);

        if (n <= 0){
            return ERROR;
        }
        total += n;
    }

    messageNetworkToHost(msg);

    return OK;
}

// Verifica o palpite do cliente e 
// preenche a mensagem de feedback com os numeros que representam o feedback.
int calculateFeedback(int *guess, int code, HackerMessage *msg) {
    int code_digits[5];
    int counter = 0;
    int remaining_digits[10] = {0};

    // printf("Code to guess: %d \n", code);
    
    // Separa o int digito a digito
    for(int i = 4; i >= 0; i--) {
        code_digits[i] = code % 10;
        code = code / 10;
    }

    // Marca acertos e ve quais digitos sobraram
    for(int i = 0; i < 5; i++) {
        if (guess[i] == code_digits[i]) {
            msg->feedback[i] = RIGHT_POSITION;
            counter++;
        }
        else {
            msg->feedback[i] = NOT_IN_CODE;
            remaining_digits[code_digits[i]]++;
        }
    }

    // printf("Digitos restantes para verificar: ");
    // for (int i = 0; i < 10; i++) {
    //     if (remaining_digits[i] > 0) {
    //         printf("%d ", i);
    //     }
    // }
    // printf("\n");

    // Verificar se os digitos restantes estão no código e
    // nao estão na posição correta
    
    for (int i = 0; i < 5; i++) {
        if (msg->feedback[i] == RIGHT_POSITION) {
            continue;
        }

        if (remaining_digits[guess[i]] > 0) {
            msg->feedback[i] = WRONG_POSITION;
            remaining_digits[guess[i]]--;
        }
    }

       // printf("Digitos restantes para verificar: ");
    // for (int i = 0; i < 10; i++) {
    //     if (remaining_digits[i] > 0) {
    //         printf("%d ", i);
    //     }
    // }
    // printf("\n");

    if(counter == 5) {
        msg->win_status = WIN;
    }
    else{
        msg->win_status = IN_GAME;
    }

    return OK;
}

// Preenche a msg de feedback com o palpite.
int fillFeedbackWithGuess(int *guess, HackerMessage *msg) {
    for(int i = 0; i < 5; i++) {
        msg->guess[i] = guess[i];
    }
    return OK;
}

int main(int argc, char **argv) {

    // Inicializa o estado da FSM
    state = START_SERVER_STATE;

    argc_counter = argc;
    code = atoi(argv[3]);

    // printf("Code to guess: %d \n", code);

    while (1)
    {
        switch (state)
        {
            case START_SERVER_STATE:
                state = SETTING_UP_SERVER_STATE;
                break;
            
            //Precisei colocar o case dentro de chaves para declarar variáveis locais
            case SETTING_UP_SERVER_STATE: {
                char *ip_type = argv[1];
                int port = atoi(argv[2]);

                server_socket = setUpServer(ip_type, port, code);

                if (server_socket < 0) {
                    return ERROR;
                }
                else{
                    memset(&msg_to_send, 0, sizeof(msg_to_send));
                    snprintf(msg_to_send.message, MSG_SIZE,
                             "Servidor iniciado em modo %s na porta %d.",
                             getProtocolType(ip_type),
                             port);
                    printf("%s\n", msg_to_send.message);
                    state = WAIT_FOR_CONNECTION_STATE;
                }

                break;
            }

            case WAIT_FOR_CONNECTION_STATE:
                client_socket = waitForClientConnection(server_socket);
                
                if (client_socket < 0) {
                    return ERROR;
                }
                else{
                    memset(&msg_to_send, 0, sizeof(msg_to_send));
                    setMessage(&msg_to_send, "Cliente conectado");
                    printf("%s\n", msg_to_send.message);
                    state = WAIT_FOR_START_MESSAGE_STATE;
                }

                break;

            case WAIT_FOR_START_MESSAGE_STATE:
                memset(&msg_received, 0, sizeof(msg_received));

                if (readMessageFromClient(client_socket, &msg_received) == ERROR) {
                    return ERROR;
                }

                if(msg_received.type == MSG_START) {
                    state = RECEIVED_START_MESSAGE_STATE;
                }
                else{
                    return ERROR;
                }

                break;

            case RECEIVED_START_MESSAGE_STATE:
                state = WAITING_FOR_MESSAGE_STATE;
                attempts_counter = 1;

                // Ao receber a msg de START, limpa as msg para uiniciar do zero.
                // Seta o attempts_counter para 1.
                memset(&msg_received, 0, sizeof(msg_received));
                memset(&msg_to_send, 0, sizeof(msg_to_send));

                break;    

            case WAITING_FOR_MESSAGE_STATE:

                // Aguarda o palpite do cliente.
                memset(&msg_received, 0, sizeof(msg_received));

                if (readMessageFromClient(client_socket, &msg_received) == ERROR) {
                    return ERROR;
                }
                else if (msg_received.type == MSG_GUESS) {
                    state = RECEIVED_GUESS_MESSAGE_STATE;
                }
                else{
                    return ERROR;
                }

                break;
            
            case RECEIVED_GUESS_MESSAGE_STATE: {
                state = SEND_FEEDBACK_STATE;

                break;
            }

            case SEND_FEEDBACK_STATE:
                memset(&msg_to_send, 0, sizeof(msg_to_send));

                if (!isValidGuess(msg_received.guess)) {
                    msg_to_send.type = MSG_ERROR;
                    msg_to_send.win_status = ERROR;
                    msg_to_send.attempts = attempts_counter;
                    setMessage(&msg_to_send, "Insira uma sequência válida!");

                    messageHostToNetwork(&msg_to_send);

                    send(client_socket, &msg_to_send, sizeof(msg_to_send), 0);

                    messageNetworkToHost(&msg_to_send);

                    state = WAITING_FOR_MESSAGE_STATE;
                    break;
                }

                fillFeedbackWithGuess(msg_received.guess, &msg_to_send);

                calculateFeedback(msg_received.guess, code, &msg_to_send);

                // Incrementa as tentativas se elas forem validas
                msg_to_send.attempts = attempts_counter++;

                if (msg_to_send.win_status == IN_GAME) {
                    msg_to_send.type = MSG_FEEDBACK;
                    char feedback[6];
                    buildFeedbackString(&msg_to_send, feedback);
                    snprintf(msg_to_send.message, MSG_SIZE,
                             "Dica: %s\nTentativas realizadas: %d",
                             feedback,
                             msg_to_send.attempts);
                }
                else if (msg_to_send.win_status == WIN) {
                    msg_to_send.type = MSG_WIN;
                    setMessage(&msg_to_send, "Acesso concedido! Thaísa recuperou o sistema!");
                }
                else {
                    return ERROR;
                }

                messageHostToNetwork(&msg_to_send);

                send(client_socket, &msg_to_send, sizeof(msg_to_send), 0);

                messageNetworkToHost(&msg_to_send);

                // Verifica o feedback para decidir o próximo estado. 
                // Se o cliente ganhou, espera a mensagem de exit. 
                //Caso contrário, espera um novo palpite.
                if(msg_to_send.win_status == IN_GAME) {
                    state = WAITING_FOR_MESSAGE_STATE;
                }
                else if (msg_to_send.win_status == WIN) {
                    state = WAIT_FOR_EXIT_MESSAGE_STATE;
                }
                else{
                    return ERROR;
                }

                break;

            case WAIT_FOR_EXIT_MESSAGE_STATE:
                memset(&msg_received, 0, sizeof(msg_received));
                if(readMessageFromClient(client_socket, &msg_received) == ERROR) {
                    return ERROR;
                }

                if (msg_received.type == MSG_EXIT) {
                    state = EXIT_STATE;
                    memset(&msg_to_send, 0, sizeof(msg_to_send));
                    setMessage(&msg_to_send, "Cliente desconectado");
                    printf("%s\n", msg_to_send.message);

                    // Fecha os sockets do cliente e do servidor antes de sair.
                    close(client_socket);
                    close(server_socket);

                }
                else{
                    return ERROR;
                }

                break;

            case EXIT_STATE:
                exit(0);
                break;

            default:
                break;
        }
    }
    return 0;
}