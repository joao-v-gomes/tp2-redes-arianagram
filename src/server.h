#ifndef SERVER_H
#define SERVER_H

#include "util.h"
#include <netinet/in.h>

#define MAX_CLIENTS 128
#define FEED_MAX_SIZE 5
#define MAX_FOLLOWS (MAX_CLIENTS * MAX_CLIENTS)

typedef struct {
    char follower[USER_SIZE];  /* quem está seguindo */
    char followed[USER_SIZE];  /* quem é seguido     */
} follow_t;

// Estados da FSM do servidor
#define START_SERVER_STATE 0
#define SETTING_UP_SERVER_STATE 1
#define WAIT_FOR_CONNECTION_STATE 2
#define EXIT_STATE 3

// Estados/etapas da thread de cliente
#define START 0
#define WAITING_FOR_MESSAGE 1
#define RECEIVED_MSG 2
#define RECEIVED_MSG_POST 3
#define RECEIVED_MSG_FOLLOW 4
#define RECEIVED_MSG_READ 5
#define RECEIVED_MSG_CONNECT 7

// Forward declaration da função que roda em thread para cada cliente
void* handleClientConnection(void *arg);
int validateInfoToSetUpServer(char *ip, int port);
int waitForClientConnection(int server_socket);
int readMessageFromClient(int client_socket, Message *msg);

bool processPostMessage(Message *msg);
bool processFollowMessage(Message *msg);
bool processReadMessage(Message *msg, int client_index);
void sendFeedToClient(int client_index);
void addMsgToFeed(Message *msg);
uint32_t incrementMsgIdCounter();
int getFeedMessages(Message *feed);

#endif // SERVER_H