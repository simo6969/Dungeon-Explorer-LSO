#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#define DIM_MAPPA 5
#define MAX_PLAYERS 4
#define MIN_PLAYERS 2
#define HP_INIZIALI 100


#define VUOTA 0
#define MOSTRO 1
#define TESORO 2
#define TRAPPOLA 3
#define USCITA 4

typedef struct {
    int tipo_contenuto;
    int esplorata;
} Stanza;

typedef struct {
    int player_id;          
    int attivo;             
    int x, y;
    int hp;
    int mossa_pronta;
    int direzione_scelta;
    
    int tesori_raccolti;
    int mostri_uccisi;
    int trappole_subite;
} Giocatore;

typedef struct {
    int id;                                  
    Stanza griglia[DIM_MAPPA][DIM_MAPPA];
    Giocatore eroi[MAX_PLAYERS];
    int num_eroi;
    int partita_finita;                      
} Dungeon;

void genera_dungeon(Dungeon *dungeon);
void aggiungi_giocatore(Dungeon *dungeon, int player_id);
void muovi_giocatore(Dungeon *dungeon, int player_index, int direzione);
void stampa_dungeon_debug(Dungeon *dungeon);

#endif
