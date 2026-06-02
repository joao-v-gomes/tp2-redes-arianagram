# 🎤 Arianagram

> Servidor multithread de uma rede social minimalista em tempo real, sobre **TCP**, com paradigma **Publish-Subscribe**.

Trabalho Prático 2 de **Redes de Computadores** (DCC/UFMG). O Arianagram é um back-end cliente-servidor onde os usuários postam pensamentos, seguem uns aos outros e recebem notificações em tempo real — o servidor age como *broker*, encaminhando cada post para os seguidores conectados.

## ✨ Funcionalidades

- **Múltiplas conexões simultâneas** — arquitetura *Single-Process, Multi-Thread*: uma thread POSIX dedicada por cliente.
- **Publish-Subscribe** — `FOLLOW` inscreve num perfil; cada `POST` dispara um `PUSH` em tempo real para os seguidores ativos.
- **Feed global** — `READ` devolve as últimas 5 mensagens (da mais nova para a mais antiga).
- **Protocolo binário de tamanho fixo** com conversão *big-endian* (`htons`/`htonl`) — interoperável entre máquinas diferentes.
- **IPv4 e IPv6**.
- **Tratamento de desconexões** — detecção de FIN via `recv()` e limpeza automática da thread.

## 🏗️ Arquitetura

```
                  +--------------------------------+
  Cliente A ----> | Thread-A                       |
  Cliente B ----> | Thread-B   Feed Global (5 msgs)|
  Cliente C ----> | Thread-C   Lista de Seguidores |
                  +--------------------------------+
                              SERVIDOR
```

O laço principal fica parado no `accept()`; cada conexão nova gera uma thread dedicada que processa os comandos daquele cliente e encaminha os pushes. As estruturas compartilhadas (clientes, feed, seguidores e contador de ID) são protegidas por mutexes.

## 🔧 Compilação

Requer `gcc`, `make` e ambiente Linux/POSIX (usa `pthreads`). Usa apenas a biblioteca padrão C e a interface POSIX.

```bash
make
```

Gera dois executáveis na raiz do projeto: `server` e `client`.

## 🚀 Execução

**Servidor** — escolha o protocolo (`v4` ou `v6`) e a porta:

```bash
./server v4 51511
```

**Cliente** — endereço, porta e seu nome de usuário (com `@`):

```bash
./client 127.0.0.1 51511 @ariana
```

O handshake (`MSG_CONNECT`) é enviado automaticamente ao conectar.

## ⌨️ Comandos do cliente

| Comando | Ação |
|---|---|
| `POST <texto>` | Publica um post (notifica os seus seguidores conectados) |
| `FOLLOW <@usuario>` | Passa a seguir um perfil |
| `READ` | Lê o feed global (até 5 mensagens, da mais nova para a mais antiga) |

## 💬 Exemplo de uso

```
# Terminal 1 — servidor
$ ./server v4 51511
Aguardando conexoes na porta 51511.
[CONN] @ariana conectou.
[CONN] @bfan conectou.
[LOG] @ariana posted (ID 1): "thank u, next"

# Terminal 2 — @ariana
$ ./client 127.0.0.1 51511 @ariana
Conectado ao servidor como @ariana.
POST thank u, next

# Terminal 3 — @bfan (segue @ariana)
$ ./client 127.0.0.1 51511 @bfan
Conectado ao servidor como @bfan.
FOLLOW @ariana
[NOTIFICATION] @ariana: "thank u, next"
READ
[FEED] ID 1 | @ariana: "thank u, next"
```

## 📦 Protocolo

Toda a comunicação usa uma struct binária de **tamanho fixo** (sem delimitadores):

```c
typedef struct {
    uint16_t type;          // MessageType (ver tabela)
    char     username[16];  // autor da mensagem
    char     content[140];  // texto do post / alvo do FOLLOW
    uint32_t msg_id;         // ID sequencial (preenchido pelo servidor)
} Message;
```

| Tipo | Valor | Sentido |
|---|:---:|---|
| `MSG_CONNECT` | 0 | Cliente → Servidor — handshake |
| `MSG_POST` | 1 | Cliente → Servidor — novo post |
| `MSG_FOLLOW` | 2 | Cliente → Servidor — seguir perfil |
| `MSG_READ` | 3 | Cliente → Servidor — pedir o feed |
| `MSG_PUSH` | 4 | Servidor → Cliente — notificação / item de feed |
| `MSG_END` | 5 | Servidor → Cliente — fim do feed |

Os campos numéricos (`type`, `msg_id`) trafegam em *network byte order* (big-endian).

## 🗂️ Estrutura do projeto

```
.
├── Makefile
├── README.md
├── src/
│   ├── server.c / server.h     # servidor multithread (pub-sub, feed, follows)
│   ├── client.c / client.h     # cliente interativo (thread leitora + entrada)
│   └── util.c   / util.h       # struct Message, endianness e tipos compartilhados
└── Documentacao Tecnica TP2 - Arianagram.md
```

## 📄 Documentação técnica

Detalhes de projeto — estruturas de dados, ciclo de vida da thread, concorrência no envio dos pushes e decisões de projeto — em **[Documentação Técnica](./Documentacao%20Tecnica%20TP2%20-%20Arianagram.md)**.

## 👤 Autor

Desenvolvido por **João** ([@joao-v-gomes](https://github.com/joao-v-gomes)) para a disciplina de **Redes de Computadores** — DCC/UFMG, 1º semestre de 2026.
