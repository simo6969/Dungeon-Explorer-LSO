#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "game_logic.h"

#define PORTA_SERVER 8080

#define MSG_SISTEMA 1
#define MSG_ERRORE 2
#define MSG_MOSSA 3  
#define MSG_STATO 4 


typedef struct {
    int tipo_messaggio;
    char payload[256];
} PacchettoRete;


typedef struct {
    int tipo_messaggio; 
    int direzione;    
} PacchettoMossa;


typedef struct {
    int tipo_messaggio;
    Dungeon mappa;      
    char log_eventi[256];
} PacchettoStato;

#endif