#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PORTA_SERVER 8080
#define MAX_PLAYERS 4

// Tipi di messaggio
#define MSG_SISTEMA 1
#define MSG_CHAT 2
#define MSG_ERRORE 3

typedef struct {
    int tipo_messaggio;
    char payload[256];
} PacchettoRete;

#endif