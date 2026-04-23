#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "protocol.h"

Dungeon partita;
pthread_mutex_t mutex_partita = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_turno = PTHREAD_COND_INITIALIZER; 
int mosse_ricevute = 0;

void trasmetti_stato_a_tutti(const char *messaggio) {
    PacchettoStato stato_out;
    stato_out.tipo_messaggio = MSG_STATO;
    stato_out.mappa = partita;
    strncpy(stato_out.log_eventi, messaggio, 255);

    for (int i = 0; i < partita.num_eroi_attivi; i++) {
        if (partita.eroi[i].hp > 0) {
            send(partita.eroi[i].socket_id, &stato_out, sizeof(PacchettoStato), 0);
        }
    }
}


void *gestisci_giocatore(void *arg) {
    int client_socket = *(int *)arg;
    free(arg); 

    pthread_mutex_lock(&mutex_partita);
    aggiungi_giocatore(&partita, client_socket);
    int mio_indice = partita.num_eroi_attivi - 1; 
    
    PacchettoStato primo_stato;
    primo_stato.tipo_messaggio = MSG_STATO;
    primo_stato.mappa = partita;
    strncpy(primo_stato.log_eventi, "Benvenuto nel Dungeon! Usa i numeri per muoverti.", 255);
    send(client_socket, &primo_stato, sizeof(PacchettoStato), 0);
    pthread_mutex_unlock(&mutex_partita);


    PacchettoMossa mossa_in;
    while (recv(client_socket, &mossa_in, sizeof(PacchettoMossa), 0) > 0) {
        
        pthread_mutex_lock(&mutex_partita); 

        partita.eroi[mio_indice].direzione_scelta = mossa_in.direzione;
        partita.eroi[mio_indice].mossa_pronta = 1;
        mosse_ricevute++;

        printf("[SERVER] Giocatore %d pronto. (%d/%d mosse ricevute)\n", 
               client_socket, mosse_ricevute, partita.num_eroi_attivi);

        if (mosse_ricevute == partita.num_eroi_attivi) {
            printf("[SERVER] Tutti pronti! Eseguo il turno...\n");
            
            // Muovo tutti quanti
            for (int i = 0; i < partita.num_eroi_attivi; i++) {
                if (partita.eroi[i].hp > 0) {
                    muovi_giocatore(&partita, i, partita.eroi[i].direzione_scelta);
                    partita.eroi[i].mossa_pronta = 0; 
                }
            }
            
            mosse_ricevute = 0; 
            trasmetti_stato_a_tutti("Turno concluso! Cosa fai adesso?");
            
            pthread_cond_broadcast(&cond_turno); 
        } 
        else {
            printf("[SERVER] Giocatore %d in attesa degli altri...\n", client_socket);
            pthread_cond_wait(&cond_turno, &mutex_partita);
        }

        pthread_mutex_unlock(&mutex_partita); 
    }

    printf("[THREAD] Giocatore %d disconnesso.\n", client_socket);
    close(client_socket);
    pthread_exit(NULL); 
}

int main() {
    setbuf(stdout, NULL); 
    
    genera_dungeon(&partita);
    stampa_dungeon_debug(&partita);

    int server_socket, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) exit(1);
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) exit(1);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORTA_SERVER);
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) exit(1);
    if (listen(server_socket, MAX_PLAYERS) < 0) exit(1);

    printf("[SERVER-INFO] Server aperto sulla porta %d.\n", PORTA_SERVER);

    while(1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen)) < 0) continue;
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_socket;
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, gestisci_giocatore, (void *)new_sock) == 0) {
            pthread_detach(thread_id);
        }
    }
    return 0;
}