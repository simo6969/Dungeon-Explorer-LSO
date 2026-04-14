#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "protocol.h"

int main() {
    // Questa riga magica disabilita il "cassetto" e forza il C 
    // a stampare tutto immediatamente nel terminale Docker.
    setbuf(stdout, NULL); 

    printf("[SERVER-INFO] Server di Dungeon Explorer in fase di avvio...\n");
    printf("[SERVER-INFO] In ascolto sulla porta %d...\n", PORTA_SERVER);
    
    // Il server resta acceso
    while(1) {
        sleep(10); 
    }
    
    return 0;
}