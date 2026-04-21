#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "protocol.h"
#include "game_logic.h"

int giocatori_connessi[MAX_PLAYERS];
int num_giocatori = 0;
pthread_mutex_t lobby_mutex = PTHREAD_MUTEX_INITIALIZER; 

void trasmetti_a_tutti(const char *messaggio) {
    PacchettoRete pacchetto;
    pacchetto.tipo_messaggio = MSG_SISTEMA;
    strncpy(pacchetto.payload, messaggio, sizeof(pacchetto.payload) - 1);

    pthread_mutex_lock(&lobby_mutex); 
    for (int i = 0; i < num_giocatori; i++) {
        send(giocatori_connessi[i], &pacchetto, sizeof(PacchettoRete), 0);
    }
    pthread_mutex_unlock(&lobby_mutex); 
}

// ---------------------------------------------------------
// Thread del singolo Giocatore
// ---------------------------------------------------------
void *gestisci_giocatore(void *arg) {
    int client_socket = *(int *)arg;
    free(arg); 

    pthread_mutex_lock(&lobby_mutex);
    giocatori_connessi[num_giocatori] = client_socket;
    num_giocatori++;
    int quanti_siamo = num_giocatori;
    pthread_mutex_unlock(&lobby_mutex);

  
    char avviso[256];
    snprintf(avviso, sizeof(avviso), "Un nuovo eroe è entrato! Siamo in %d/%d nella lobby.", quanti_siamo, MAX_PLAYERS);
    printf("[SERVER] %s\n", avviso);
    trasmetti_a_tutti(avviso);

    PacchettoRete pacchetto_in;
    while (recv(client_socket, &pacchetto_in, sizeof(PacchettoRete), 0) > 0) {
        printf("[GIOCATORE %d DICE]: %s\n", client_socket, pacchetto_in.payload);
    }

    printf("[THREAD] Il giocatore %d si è disconnesso.\n", client_socket);
    
    pthread_mutex_lock(&lobby_mutex);
    num_giocatori--;
    pthread_mutex_unlock(&lobby_mutex);

    close(client_socket);
    pthread_exit(NULL); 
}

// ---------------------------------------------------------
// MAIN
// ---------------------------------------------------------
int main() {
    setbuf(stdout, NULL); 
    int server_socket, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) exit(EXIT_FAILURE);
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) exit(EXIT_FAILURE);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORTA_SERVER);

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) exit(EXIT_FAILURE);
    if (listen(server_socket, MAX_PLAYERS) < 0) exit(EXIT_FAILURE);

    Dungeon mappa_partita;
    genera_dungeon(&mappa_partita);
    stampa_dungeon_debug(&mappa_partita);

    printf("[SERVER-INFO] Lobby aperta. In attesa di %d giocatori...\n", MAX_PLAYERS);

    while(1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen)) < 0) continue;

        pthread_mutex_lock(&lobby_mutex);
        if (num_giocatori >= MAX_PLAYERS) {
            PacchettoRete msg_rifiuto;
            msg_rifiuto.tipo_messaggio = MSG_ERRORE;
            strncpy(msg_rifiuto.payload, "Lobby piena! Riprova più tardi.", 255);
            send(client_socket, &msg_rifiuto, sizeof(PacchettoRete), 0);
            close(client_socket);
            pthread_mutex_unlock(&lobby_mutex);
            continue;
        }
        pthread_mutex_unlock(&lobby_mutex);

        int *new_sock = malloc(sizeof(int));
        *new_sock = client_socket;
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, gestisci_giocatore, (void *)new_sock) == 0) {
            pthread_detach(thread_id);
        }
    }
    return 0;
}