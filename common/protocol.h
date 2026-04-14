#ifndef PROTOCOL_H
#define PROTOCOL_H

// Costanti di base
#define PORTA_SERVER 8080
#define MAX_PLAYERS 4

// Struttura base per un messaggio (la espanderemo in seguito)
typedef struct {
    int tipo_messaggio; // Es: 1 = Connessione, 2 = Mossa, 3 = Chat
    char payload[256];  // Il contenuto testuale del messaggio
} PacchettoRete;

#endif