#include "util.h"

//Defines para a FSM do cliente
#define START_CLIENT_STATE 0
#define CONNECT_TO_SERVER_STATE 1
#define SEND_START_MESSAGE_STATE 2
#define SEND_GUESS_STATE 3
#define WAIT_FOR_FEEDBACK_STATE 4
#define RECEIVED_IN_GAME_FEEDBACK_STATE 5
#define RECEIVED_WIN_FEEDBACK_STATE 6
#define RECEIVED_ERROR_FEEDBACK_STATE 7
#define WIN_STATE 8
#define EXIT_STATE 9


// Protótipos das funções do cliente
int validateInfoToConnectToServer(char *server_ip, int server_port);
void messageHostToNetwork(HackerMessage *msg);
void messageNetworkToHost(HackerMessage *msg);
int isValidGuess(const char *guess_string);
void convertFeedback(HackerMessage *msg_received, char *feedback);
int readMessageFromServer(int client_socket, HackerMessage *msg_received);
int connectToServer(char *server_ip, int server_port);
int validateInfoToConnectToServer(char *server_ip, int server_port);