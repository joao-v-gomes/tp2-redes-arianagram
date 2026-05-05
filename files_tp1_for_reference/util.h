#define MSG_SIZE 128

//Retorno das dicas
#define RIGHT_POSITION 2
#define WRONG_POSITION 1
#define NOT_IN_CODE 0

//Status do jogo
#define WIN 1
#define IN_GAME 0

// Defines para os returns das funcoes
#define OK 0
#define ERROR -1

// Struct de MessageType
typedef enum {
    MSG_START, // Servidor solicita a senha de acesso
    MSG_GUESS, // Cliente envia o palpite de 5 dígitos
    MSG_FEEDBACK, // Servidor retorna as dicas ( 0 , 1 ou 2)
    MSG_WIN, // Servidor informa que o código foi quebrado
    MSG_ERROR, // Servidor reporta erro de forma to / tamanho
    MSG_EXIT // Encerramento da conexao
} MessageType ;

// Struct de HackerMessage
typedef struct {
    int type ; // Tipo da mensagem ( MessageType )
    int guess [ 5 ] ; // Vetor com os 5 dígitos enviados pelo cliente
    int feedback [ 5 ] ; // Dicas retornadas pelo servidor ( 2 , 1 ou 0)
    int attempts ; // Contador de tentativas realizadas
    int win_status ; // 1 para vitória , 0 para em jogo , −1 para erro
    char message [ MSG_SIZE ] ; // Mensagem de texto para logs do terminal
} HackerMessage ;