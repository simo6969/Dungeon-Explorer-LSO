#include <stdio.h>
#include <stdlib.h>
#include "../common/game_logic.h"

void genera_dungeon(Dungeon *dungeon) {
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
    dungeon->num_eroi = 0;
    dungeon->partita_finita = 0;
}

void stampa_dungeon_debug(Dungeon *dungeon) {
    printf("\n=== MAPPA SEGRETA DEL DUNGEON #%d (DEBUG) ===\n", dungeon->id);
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

void aggiungi_giocatore(Dungeon *dungeon, int player_id) {
    Giocatore *g = &dungeon->eroi[player_id];
    g->player_id = player_id;
    g->attivo = 1;
    g->x = 0;
    g->y = 0;
    g->hp = HP_INIZIALI;
    g->mossa_pronta = 0;
    g->direzione_scelta = 0;
    g->tesori_raccolti = 0;
    g->mostri_uccisi = 0;
    g->trappole_subite = 0;

    if (player_id >= dungeon->num_eroi) {
        dungeon->num_eroi = player_id + 1;
    }
    printf("[GAME] Dungeon %d: giocatore %d spawnato a (0,0).\n", dungeon->id, player_id);
}


void muovi_giocatore(Dungeon *dungeon, int player_index, int direzione) {
    Giocatore *eroe = &dungeon->eroi[player_index];

    int nuova_x = eroe->x;
    int nuova_y = eroe->y;

    
    if (direzione == 1) nuova_y--;
    else if (direzione == 2) nuova_y++;
    else if (direzione == 3) nuova_x++;
    else if (direzione == 4) nuova_x--;

    if (nuova_x < 0 || nuova_x >= DIM_MAPPA || nuova_y < 0 || nuova_y >= DIM_MAPPA) {
        printf("[GAME] Dungeon %d: il giocatore %d ha sbattuto contro il muro!\n",
               dungeon->id, eroe->player_id);
        return;
    }

    eroe->x = nuova_x;
    eroe->y = nuova_y;

    Stanza *nuova_stanza = &dungeon->griglia[nuova_y][nuova_x];
    nuova_stanza->esplorata = 1;

    switch (nuova_stanza->tipo_contenuto) {
        case MOSTRO:
            eroe->hp -= 20;
            eroe->mostri_uccisi++;
            printf("[GAME] Dungeon %d: giocatore %d attaccato da Mostro! (-20 HP, ora %d).\n",
                   dungeon->id, eroe->player_id, eroe->hp);
            nuova_stanza->tipo_contenuto = VUOTA;
            break;

        case TRAPPOLA:
            eroe->hp -= 10;
            eroe->trappole_subite++;
            printf("[GAME] Dungeon %d: giocatore %d in Trappola! (-10 HP, ora %d).\n",
                   dungeon->id, eroe->player_id, eroe->hp);
            nuova_stanza->tipo_contenuto = VUOTA;
            break;

        case TESORO:
            eroe->hp += 15;
            if (eroe->hp > 100) eroe->hp = 100;
            eroe->tesori_raccolti++;
            printf("[GAME] Dungeon %d: giocatore %d ha trovato una Pozione! (+15 HP, ora %d).\n",
                   dungeon->id, eroe->player_id, eroe->hp);
            nuova_stanza->tipo_contenuto = VUOTA;
            break;

        case USCITA:
            printf("\n[GAME] Dungeon %d: GIOCATORE %d HA TROVATO L'USCITA!\n",
                   dungeon->id, eroe->player_id);
            dungeon->partita_finita = 1;
            break;
    }

    if (eroe->hp <= 0) {
        eroe->attivo = 0;
        printf("[GAME] Dungeon %d: giocatore %d e' caduto.\n", dungeon->id, eroe->player_id);
    }
}
