#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include "util.h"

#define START_CLIENT_STATE 0
#define CONNECT_TO_SERVER_STATE 1
#define WAIT_USER_INPUT_STATE 2
#define VALIDATE_USER_INPUT_STATE 3
#define SEND_MSG_TO_SERVER_STATE 4
#define EXIT_STATE 5



int connectToServer(char *server_ip, int server_port, char *username);
int validateInfoToConnectToServer(char *server_ip, int server_port, char *username);
int readMessageFromServer(int client_socket, Message *msg);

char *getUsername();
void setUsername(char *new_username);

#endif // CLIENT_H
