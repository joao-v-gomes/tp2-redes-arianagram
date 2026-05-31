#include "util.h"

// Trata o problema da endianess do Host/Rede,
// convertendo os campos da mensagem de host para network byte order antes de enviar,
// e de network para host byte order após receber.
void messageHostToNetwork(Message *msg) {
    msg->type   = htons(msg->type);
    msg->msg_id = htonl(msg->msg_id);
    // username e content são char[], sem conversão de byte order necessária.
}

void messageNetworkToHost(Message *msg) {
    msg->type   = ntohs(msg->type);
    msg->msg_id = ntohl(msg->msg_id);
}

void printMsg(Message *msg) {
    printf("Mensagem: Id: %d, Tipo: %d, Usuário: %s, Conteúdo: %s\n", msg->msg_id, msg->type, msg->username, msg->content);
}