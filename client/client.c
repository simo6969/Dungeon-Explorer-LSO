#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "protocol.h"

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    printf("[CLIENT-INFO] Benvenuto in Dungeon Explorer!\n");
    printf("[CLIENT-INFO] Preparazione dell'equipaggiamento di rete...\n");

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[CLIENT-ERR] Errore nella creazione del socket\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORTA_SERVER);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("[CLIENT-ERR] Indirizzo IP non valido o non supportato\n");
        return -1;
    }

    printf("[CLIENT-INFO] Tentativo di connessione al Server (127.0.0.1:%d)...\n", PORTA_SERVER);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[CLIENT-ERR] Connessione fallita. Il Server è acceso?\n");
        return -1;
    }

    printf("[CLIENT-INFO] Connessione stabilita con successo! Sei nel Dungeon.\n");

    close(sock);
    
    printf("[CLIENT-INFO] Disconnessione. A presto!\n");

    return 0;
}