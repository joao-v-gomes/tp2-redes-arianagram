#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#define OK 0
#define ERROR -1

#define CONTENT_SIZE 140
#define USER_SIZE 16


// Tipos de mensagens entre cliente e servidor
typedef enum {
    MSG_CONNECT = 0,    /* Cliente -> Servidor : conectar-se ao servidor */
    MSG_POST = 1,       /* Cliente -> Servidor : enviar novo post */
    MSG_FOLLOW = 2,     /* Cliente -> Servidor : seguir outro usuario */
    MSG_READ = 3,       /* Cliente -> Servidor : solicitar feed global */
    MSG_PUSH = 4,        /* Servidor -> Cliente : notificacao de seguido */
    MSG_END = 5         /* Servidor -> Cliente: fim do feed */
} MessageType;

// Estrutura da mensagem entre cliente e servidor
typedef struct {
    uint16_t type ;                 /* Tipo da mensagem ( MessageType ) */
    char username [ USER_SIZE ];    /* Nome do autor (ex: " @ariana ") */
    char content [ CONTENT_SIZE ];  /* Texto da mensagem ou alvo do FOLLOW */
    uint32_t msg_id ;               /* ID sequencial ( preenchido pelo servidor ) */
} Message;

// Estrutura para armazenar as informações de um cliente conectado.
typedef struct {
    int active;

    int client_fd;
    pthread_t id;

    char username[USER_SIZE];

    // Protege escritas no socket deste cliente (thread do poster + thread própria podem escrever concorrentemente)
    pthread_mutex_t send_mutex;

} client_t;

void messageHostToNetwork(Message *msg);
void messageNetworkToHost(Message *msg);
void printMsg(Message *msg);

#endif // UTIL_H