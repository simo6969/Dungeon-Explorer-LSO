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