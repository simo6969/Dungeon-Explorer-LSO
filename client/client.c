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

    printf("[CLIENT-INFO] Connessione al Dungeon in corso...\n");

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORTA_SERVER);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) return -1;

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[CLIENT-ERR] Server irraggiungibile.\n");
        return -1;
    }

    printf("=========================================\n");
    printf("        SEI ENTRATO NELLA LOBBY!         \n");
    printf("=========================================\n");

    // Ciclo infinito di ascolto: aspettiamo avvisi dal Server
    PacchettoRete pacchetto_in;
    while (1) {
        int bytes_ricevuti = recv(sock, &pacchetto_in, sizeof(PacchettoRete), 0);
        
        if (bytes_ricevuti <= 0) {
            printf("\n[CLIENT-ERR] Connessione persa con il Server.\n");
            break; // Usciamo dal ciclo se il server crasha o ci sbatte fuori
        }

        // Stampiamo il messaggio in base al tipo
        if (pacchetto_in.tipo_messaggio == MSG_SISTEMA) {
            printf(">> [SISTEMA]: %s\n", pacchetto_in.payload);
        } else if (pacchetto_in.tipo_messaggio == MSG_ERRORE) {
            printf(">> [ERRORE]: %s\n", pacchetto_in.payload);
            break; // Usciamo se la lobby era piena
        }
    }

    close(sock);
    return 0;
}