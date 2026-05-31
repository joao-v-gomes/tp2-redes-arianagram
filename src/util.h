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

// // Estrutura para representar uma mensagem na fila de mensagens
// // importante pra poder iterar sobre a fila e liberar a memória alocada para cada mensagem depois de processada e pra organizar as msg que chegarem
// typedef struct msg_node {
//     Message msg;
//     struct msg_node *next;
// } msg_node_t;

// Estrutura para armazenar as informações de um cliente conectado
// cada cliente tem uma fila de mensagens para receber do servidor, protegida por um mutex e uma variável de condição para sincronização.
typedef struct {
    int active;

    int client_fd;
    pthread_t id;

    char username[USER_SIZE];

    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;

    // msg_node_t *queue_head;
    // msg_node_t *queue_tail;

} client_t;

void messageHostToNetwork(Message *msg);
void messageNetworkToHost(Message *msg);
void printMsg(Message *msg);

#endif // UTIL_H