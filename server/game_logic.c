#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../common/game_logic.h"

void genera_dungeon(Dungeon *dungeon) {
    srand(time(NULL));

    for (int riga = 0; riga < DIM_MAPPA; riga++) {
        for (int col = 0; col < DIM_MAPPA; col++) {
            
            dungeon->griglia[riga][col].esplorata = 0; 

            if (riga == 0 && col == 0) {
                dungeon->griglia[riga][col].tipo_contenuto = VUOTA;
                dungeon->griglia[riga][col].esplorata = 1; 
            }
            else if (riga == DIM_MAPPA - 1 && col == DIM_MAPPA - 1) {
                dungeon->griglia[riga][col].tipo_contenuto = USCITA;
            }
            else {
                int probabilita = rand() % 100; 

                if (probabilita < 50) {
                    dungeon->griglia[riga][col].tipo_contenuto = VUOTA;       
                } else if (probabilita < 80) {
                    dungeon->griglia[riga][col].tipo_contenuto = MOSTRO;     
                } else if (probabilita < 90) {
                    dungeon->griglia[riga][col].tipo_contenuto = TESORO;      
                } else {
                    dungeon->griglia[riga][col].tipo_contenuto = TRAPPOLA;    
                }
            }
        }
    }
}

void stampa_dungeon_debug(Dungeon *dungeon) {
    printf("\n=== MAPPA SEGRETA DEL DUNGEON (DEBUG) ===\n");
    for (int riga = 0; riga < DIM_MAPPA; riga++) {
        for (int col = 0; col < DIM_MAPPA; col++) {
            int tipo = dungeon->griglia[riga][col].tipo_contenuto;
            switch(tipo) {
                case VUOTA:    printf("[ . ] "); break;
                case MOSTRO:   printf("[ M ] "); break;
                case TESORO:   printf("[ T ] "); break;
                case TRAPPOLA: printf("[ X ] "); break;
                case USCITA:   printf("[ U ] "); break;
            }
        }
        printf("\n"); 
    }
    printf("=========================================\n\n");
}

void aggiungi_giocatore(Dungeon *dungeon, int socket_id) {
    int index = dungeon->num_eroi_attivi;
    
    dungeon->eroi[index].socket_id = socket_id;
    dungeon->eroi[index].x = 0; // Tutti nascono in alto a sinistra (0,0)
    dungeon->eroi[index].y = 0;
    dungeon->eroi[index].hp = HP_INIZIALI;
    dungeon->eroi[index].mossa_pronta = 0; // Ancora non ha deciso cosa fare
    
    dungeon->num_eroi_attivi++;
    dungeon->partita_finita = 0;
    printf("[GAME] Giocatore %d spawnato alle coordinate (0,0).\n", socket_id);
}


void muovi_giocatore(Dungeon *dungeon, int player_index, int direzione) {
    Giocatore *eroe = &dungeon->eroi[player_index];
    
    int nuova_x = eroe->x;
    int nuova_y = eroe->y;

    // Calcoliamo le nuove coordinate in base alla direzione (1=N, 2=S, 3=E, 4=O)
    if (direzione == 1) nuova_y--;      // Nord (su)
    else if (direzione == 2) nuova_y++; // Sud (giù)
    else if (direzione == 3) nuova_x++; // Est (destra)
    else if (direzione == 4) nuova_x--; // Ovest (sinistra)

    // Controllo dei bordi della mappa (non possiamo uscire dal 5x5)
    if (nuova_x < 0 || nuova_x >= DIM_MAPPA || nuova_y < 0 || nuova_y >= DIM_MAPPA) {
        printf("[GAME] Il giocatore %d ha sbattuto contro il muro!\n", eroe->socket_id);
        return; // Mossa invalida, resta fermo
    }

    // Aggiorniamo le coordinate
    eroe->x = nuova_x;
    eroe->y = nuova_y;
    
    // Scopriamo cosa c'è nella nuova stanza
    Stanza *nuova_stanza = &dungeon->griglia[nuova_y][nuova_x];
    nuova_stanza->esplorata = 1; // Ora è visibile a tutti

    // Gestione Eventi
    switch (nuova_stanza->tipo_contenuto) {
        case MOSTRO:
            eroe->hp -= 20;
            printf("[GAME] Il giocatore %d è stato attaccato da un Mostro! (-20 HP). HP Rimasti: %d\n", eroe->socket_id, eroe->hp);
            // Il mostro è sconfitto, la stanza diventa vuota
            nuova_stanza->tipo_contenuto = VUOTA; 
            break;
            
        case TRAPPOLA:
            eroe->hp -= 10;
            printf("[GAME] Il giocatore %d è finito in una Trappola! (-10 HP). HP Rimasti: %d\n", eroe->socket_id, eroe->hp);
            nuova_stanza->tipo_contenuto = VUOTA;
            break;
            
        case TESORO:
            eroe->hp += 15;
            if (eroe->hp > 100) eroe->hp = 100; // Cap massimo a 100
            printf("[GAME] Il giocatore %d ha trovato una Pozione! (+15 HP). HP Rimasti: %d\n", eroe->socket_id, eroe->hp);
            nuova_stanza->tipo_contenuto = VUOTA;
            break;
            
        case USCITA:
            printf("\n🏆 [GAME] IL GIOCATORE %d HA TROVATO L'USCITA! 🏆\n", eroe->socket_id);
            dungeon->partita_finita = 1;
            break;
    }
}