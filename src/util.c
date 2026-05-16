#include "util.h"

// Trata o problema da endianess do Host/Rede,
// convertendo os campos da mensagem de host para network byte order antes de enviar,
// e de network para host byte order após receber.
void messageHostToNetwork(Message *msg) {
    // msg->type = htonl(msg->type);
    // for (int i = 0; i < 5; i++) {
    //     msg->guess[i] = htonl(msg->guess[i]);
    //     msg->feedback[i] = htonl(msg->feedback[i]);
    // }
    // msg->attempts = htonl(msg->attempts);
    // msg->win_status = htonl(msg->win_status);
}

void messageNetworkToHost(Message *msg) {
    // msg->type = ntohl(msg->type);
    // for (int i = 0; i < 5; i++) {
    //     msg->guess[i] = ntohl(msg->guess[i]);
    //     msg->feedback[i] = ntohl(msg->feedback[i]);
    // }
    // msg->attempts = ntohl(msg->attempts);
    // msg->win_status = ntohl(msg->win_status);
}

void printMsg(Message *msg) {
    printf("Mensagem: Id: %d, Tipo: %d, Usuário: %s, Conteúdo: %s\n", msg->msg_id, msg->type, msg->username, msg->content);
}