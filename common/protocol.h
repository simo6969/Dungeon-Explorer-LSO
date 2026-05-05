#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "game_logic.h"

#define PORTA_SERVER 8080
#define MSG_SISTEMA           1
#define MSG_ERRORE            2
#define MSG_CREA_DUNGEON           10  
#define MSG_ENTRA_DUNGEON          11  
#define MSG_LOBBY_OK               12  
#define MSG_LOBBY_ERRORE           13  
#define MSG_OWNER_PROMPT_RICHIESTA 30  
#define MSG_OWNER_ACCETTA          31  
#define MSG_OWNER_RIFIUTA          32  
#define MSG_OWNER_PROMPT_START     33  
#define MSG_OWNER_START            34  
#define MSG_OWNER_CONTINUA         35  
#define MSG_LOBBY_GAME_START       36  
#define MSG_MOSSA                  20
#define MSG_STATO                  21

typedef struct {
    int tipo_messaggio;
    int dungeon_id;          
    int mio_player_id;       
    char payload[256];       
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
