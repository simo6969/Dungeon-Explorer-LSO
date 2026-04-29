#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "protocol.h"

void disegna_schermo(Dungeon *d) {
    printf("\n================ IL DUNGEON ================\n");
    for (int riga = 0; riga < DIM_MAPPA; riga++) {
        for (int col = 0; col < DIM_MAPPA; col++) {
            
            int c_e_giocatore = 0;
            for(int i = 0; i < d->num_eroi_attivi; i++) {
                if(d->eroi[i].hp > 0 && d->eroi[i].x == col && d->eroi[i].y == riga) {
                    c_e_giocatore = 1;
                }
            }

            if (c_e_giocatore) {
                printf("[ P ] "); 
            } else if (d->griglia[riga][col].esplorata == 0) {
                printf("[ ? ] "); 
            } else {
                int tipo = d->griglia[riga][col].tipo_contenuto;
                if(tipo == VUOTA) printf("[ . ] ");
                else if(tipo == MOSTRO) printf("[ M ] ");
                else if(tipo == TESORO) printf("[ T ] ");
                else if(tipo == TRAPPOLA) printf("[ X ] ");
                else if(tipo == USCITA) printf("[ U ] ");
            }
        }
        printf("\n");
    }
    printf("============================================\n");
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORTA_SERVER);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) return -1;

    printf("Connessione al Server in corso...\n");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) return -1;

    PacchettoStato stato_in;
    
    while (recv(sock, &stato_in, sizeof(PacchettoStato), 0) > 0) {
        
        system("clear"); 
        
        printf(">>> MESSAGGIO: %s\n", stato_in.log_eventi);
        disegna_schermo(&stato_in.mappa);

        if (stato_in.mappa.partita_finita == 1) {
            printf("\n🎉 MISSIONE COMPIUTA! Il gruppo e' salvo. Siete degli eroi!\n\n");
            break; 
        } else if (stato_in.mappa.partita_finita == -1) {
            printf("\n💀 GAME OVER! Tutti gli eroi sono morti nel dungeon...\n\n");
            break; 
        }

        int scelta = 0;
        printf("\nE' il tuo turno!\n");
        printf("1. Nord (Su)\n2. Sud (Giu')\n3. Est (Destra)\n4. Ovest (Sinistra)\n");
        printf("Mossa: ");
        scanf("%d", &scelta);

        PacchettoMossa mossa_out;
        mossa_out.tipo_messaggio = MSG_MOSSA;
        mossa_out.direzione = scelta;
        send(sock, &mossa_out, sizeof(PacchettoMossa), 0);
        
        printf("\n[!] Mossa inviata! In attesa che gli altri giocatori scelgano...\n");
    }

    printf("\nConnessione chiusa.\n");
    close(sock);
    return 0;
}