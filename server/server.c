#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h> 
#include "protocol.h"


void *gestisci_giocatore(void *arg) {
    int client_socket = *(int *)arg;
    
    free(arg); 

    printf("[THREAD] Nuovo thread avviato! Mi sto occupando del giocatore sul socket %d\n", client_socket);

    printf("[THREAD] Il giocatore %d sta esplorando...\n", client_socket);
    sleep(15); 

    printf("[THREAD] Il giocatore %d ha abbandonato il server. Chiudo la sua connessione.\n", client_socket);
    close(client_socket);

    pthread_exit(NULL); 
}

int main() {
    setbuf(stdout, NULL);

    int server_socket, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    printf("[SERVER-INFO] Avvio Server Multi-Thread...\n");

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("[SERVER-ERR] Creazione socket fallita");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("[SERVER-ERR] Setsockopt fallita");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORTA_SERVER);

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[SERVER-ERR] Bind fallito");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_PLAYERS) < 0) {
        perror("[SERVER-ERR] Listen fallito");
        exit(EXIT_FAILURE);
    }

    printf("[SERVER-INFO] Il Centralino è aperto sulla porta %d...\n", PORTA_SERVER);

    while(1) {
        printf("[SERVER-INFO] In attesa di nuovi giocatori...\n");

        if ((client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("[SERVER-ERR] Accept fallita");
            continue; 
        }

        printf("[SERVER-INFO] Un giocatore ha bussato alla porta! Affido il giocatore a un Thread...\n");

        int *new_sock = malloc(sizeof(int));
        *new_sock = client_socket;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, gestisci_giocatore, (void *)new_sock) < 0) {
            perror("[SERVER-ERR] Impossibile creare il thread");
            free(new_sock);
            close(client_socket);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(server_socket);
    return 0;
}