#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "game_logic.h"

#define PORTA_SERVER 8080

// --- Messaggi di sistema ---
#define MSG_SISTEMA           1
#define MSG_ERRORE            2

// --- Messaggi di lobby (handshake iniziale) ---
#define MSG_CREA_DUNGEON     10  // client -> server: voglio creare un nuovo dungeon
#define MSG_ENTRA_DUNGEON    11  // client -> server: voglio entrare nel dungeon con id X
#define MSG_LOBBY_OK         12  // server -> client: ingresso confermato
#define MSG_LOBBY_ERRORE     13  // server -> client: ingresso rifiutato (con motivo)

// --- Messaggi di partita ---
#define MSG_MOSSA            20
#define MSG_STATO            21


// Pacchetto di lobby: usato sia in richiesta sia in risposta.
typedef struct {
    int tipo_messaggio;
    int dungeon_id;          // input per ENTRA, output per LOBBY_OK
    int mio_player_id;       // valorizzato dal server in LOBBY_OK
    char payload[256];       // testo libero (motivo errore, info)
} PacchettoLobby;


typedef struct {
    int tipo_messaggio;
    int direzione;
} PacchettoMossa;


typedef struct {
    int tipo_messaggio;
    int dungeon_id;
    int mio_player_id;
    Dungeon mappa;
    char log_eventi[256];
} PacchettoStato;

#endif
