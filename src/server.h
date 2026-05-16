#ifndef SERVER_H
#define SERVER_H

#include "util.h"
#include <netinet/in.h>

#define MAX_CLIENTS 5
#define FEED_MAX_SIZE 5

// Estados da FSM do servidor
#define START_SERVER_STATE 0
#define SETTING_UP_SERVER_STATE 1
#define WAIT_FOR_CONNECTION_STATE 2
#define WAIT_FOR_START_MESSAGE_STATE 3
#define RECEIVED_START_MESSAGE_STATE 4
#define WAITING_FOR_MESSAGE_STATE 5
#define RECEIVED_GUESS_MESSAGE_STATE 6
#define SEND_FEEDBACK_STATE 7
#define WAIT_FOR_EXIT_MESSAGE_STATE 8
#define EXIT_STATE 9

// Estados/etapas da thread de cliente
#define START 0
#define WAITING_FOR_MESSAGE 1
#define RECEIVED_MSG 2
#define RECEIVED_MSG_POST 3
#define RECEIVED_MSG_FOLLOW 4
#define RECEIVED_MSG_READ 5
#define SEND_MSG_TO_CLIENT 6

// Forward declaration da função que roda em thread para cada cliente
void* handleClientConnection(void *arg);
int validateInfoToSetUpServer(char *ip, int port);
int waitForClientConnection(int server_socket);
int readMessageFromClient(int client_socket, Message *msg);
int sendMessageToClient(int client_socket, Message *msg);

bool processPostMessage(Message *msg);
bool processFollowMessage(Message *msg);
bool processReadMessage(Message *msg);
void sendFeedToClient(int client_socket);
int getSocketByUsername(char *username);
void addMsgToFeed(Message *msg);
void incrementMsgIdCounter();

#endif // SERVER_H