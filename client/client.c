#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "protocol.h"

static int recv_all(int sock, void *buf, size_t len) {
    size_t totale = 0;
    char *p = (char *)buf;
    while (totale < len) {
        ssize_t n = recv(sock, p + totale, len - totale, 0);
        if (n <= 0) return -1;
        totale += (size_t)n;
    }
    return 0;
}

static int send_all(int sock, const void *buf, size_t len) {
    size_t totale = 0;
    const char *p = (const char *)buf;
    while (totale < len) {
        ssize_t n = send(sock, p + totale, len - totale, 0);
        if (n <= 0) return -1;
        totale += (size_t)n;
    }
    return 0;
}

static void disegna_schermo(const Dungeon *d, int mio_pid) {
    printf("\n============ DUNGEON #%d ============\n", d->id);
    for (int riga = 0; riga < DIM_MAPPA; riga++) {
        for (int col = 0; col < DIM_MAPPA; col++) {

            int pid_in_stanza = -1;
            for (int i = 0; i < d->num_eroi; i++) {
                if (d->eroi[i].hp > 0 && d->eroi[i].x == col && d->eroi[i].y == riga) {
                    pid_in_stanza = i;
                    break;
                }
            }

            if (pid_in_stanza == mio_pid && pid_in_stanza >= 0) {
                printf("[ @ ] ");
            } else if (pid_in_stanza >= 0) {
                printf("[ %d ] ", pid_in_stanza);
            } else if (d->griglia[riga][col].esplorata == 0) {
                printf("[ ? ] ");
            } else {
                int tipo = d->griglia[riga][col].tipo_contenuto;
                if      (tipo == VUOTA)    printf("[ . ] ");
                else if (tipo == MOSTRO)   printf("[ M ] ");
                else if (tipo == TESORO)   printf("[ T ] ");
                else if (tipo == TRAPPOLA) printf("[ X ] ");
                else if (tipo == USCITA)   printf("[ U ] ");
            }
        }
        printf("\n");
    }
    printf("=====================================\n");
    Giocatore me = d->eroi[mio_pid];
    printf("Tu (player %d): HP=%d  tesori=%d  mostri=%d  trappole=%d\n",
           mio_pid, me.hp, me.tesori_raccolti, me.mostri_uccisi, me.trappole_subite);
}

static int leggi_intero(const char *prompt) {
    int x;
    char buf[64];
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) exit(1);
        if (sscanf(buf, "%d", &x) == 1) return x;
        printf("Input non valido, riprova.\n");
    }
}

static int gestisci_lobby_owner_lato_client(int sock) {
    while (1) {
        PacchettoLobby in;
        if (recv_all(sock, &in, sizeof(in)) < 0) {
            printf("Connessione persa.\n");
            return -1;
        }

        if (in.tipo_messaggio == MSG_LOBBY_GAME_START) {
            printf("\n[!] %s\n", in.payload);
            return 0;
        }

        if (in.tipo_messaggio == MSG_LOBBY_ERRORE) {
            printf("[ERRORE] %s\n", in.payload);
            return -1;
        }

        PacchettoLobby out;
        memset(&out, 0, sizeof(out));

        if (in.tipo_messaggio == MSG_OWNER_PROMPT_RICHIESTA) {
            printf("\n[OWNER] %s\n", in.payload);
            int scelta = leggi_intero("> ");
            out.tipo_messaggio = (scelta == 1) ? MSG_OWNER_ACCETTA : MSG_OWNER_RIFIUTA;
        }
        else if (in.tipo_messaggio == MSG_OWNER_PROMPT_START) {
            printf("\n[OWNER] %s\n", in.payload);
            int scelta = leggi_intero("> ");
            out.tipo_messaggio = (scelta == 1) ? MSG_OWNER_START : MSG_OWNER_CONTINUA;
        }
        else {
            printf("[CLIENT] Messaggio lobby inatteso: %d\n", in.tipo_messaggio);
            return -1;
        }

        if (send_all(sock, &out, sizeof(out)) < 0) return -1;
    }
}

static int attendi_game_start(int sock) {
    PacchettoLobby in;
    if (recv_all(sock, &in, sizeof(in)) < 0) {
        printf("Connessione persa in attesa dell'avvio.\n");
        return -1;
    }
    if (in.tipo_messaggio == MSG_LOBBY_GAME_START) {
        printf("\n[!] %s\n", in.payload);
        return 0;
    }
    if (in.tipo_messaggio == MSG_LOBBY_ERRORE) {
        printf("[ERRORE] %s\n", in.payload);
        return -1;
    }
    printf("[CLIENT] Messaggio inatteso in attesa di GAME_START: %d\n", in.tipo_messaggio);
    return -1;
}

int main(void) {
    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return 1;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORTA_SERVER);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) return 1;

    printf("Connessione al server in corso...\n");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        return 1;
    } 
    printf("\n=== DUNGEON EXPLORER ===\n");
    printf("1. Crea un nuovo dungeon (sarai il proprietario)\n");
    printf("2. Entra in un dungeon esistente\n");
    int scelta = leggi_intero("Scelta: ");

    PacchettoLobby req;
    memset(&req, 0, sizeof(req));
    int sono_owner = 0;
    if (scelta == 1) {
        req.tipo_messaggio = MSG_CREA_DUNGEON;
        sono_owner = 1;
    } else if (scelta == 2) {
        req.tipo_messaggio = MSG_ENTRA_DUNGEON;
        req.dungeon_id = leggi_intero("ID del dungeon: ");
    } else {
        printf("Scelta non valida.\n");
        close(sock);
        return 1;
    }
    if (send_all(sock, &req, sizeof(req)) < 0) { close(sock); return 1; }

    PacchettoLobby resp;
    if (recv_all(sock, &resp, sizeof(resp)) < 0) {
        printf("Connessione persa durante la lobby.\n");
        close(sock);
        return 1;
    }
    if (resp.tipo_messaggio == MSG_LOBBY_ERRORE) {
        printf("[ERRORE] %s\n", resp.payload);
        close(sock);
        return 1;
    }
    if (resp.tipo_messaggio != MSG_LOBBY_OK) {
        printf("Messaggio inatteso dal server: %d\n", resp.tipo_messaggio);
        close(sock);
        return 1;
    }
    printf("[OK] %s\n", resp.payload);

    
    if (sono_owner) {
        if (gestisci_lobby_owner_lato_client(sock) < 0) {
            close(sock);
            return 1;
        }
    } else {
        if (attendi_game_start(sock) < 0) {
            close(sock);
            return 1;
        }
    }

    
    PacchettoStato stato_in;
    while (recv_all(sock, &stato_in, sizeof(stato_in)) == 0) {
        printf("\n>>> %s\n", stato_in.log_eventi);
        disegna_schermo(&stato_in.mappa, stato_in.mio_player_id);

        if (stato_in.mappa.partita_finita == 1) {
            printf("\nMISSIONE COMPIUTA! Il gruppo e' salvo.\n\n");
            break;
        } else if (stato_in.mappa.partita_finita == -1) {
            printf("\nGAME OVER! Tutti gli eroi sono morti.\n\n");
            break;
        }

        printf("\n1. Nord  2. Sud  3. Est  4. Ovest\n");
        int dir = leggi_intero("Mossa: ");

        PacchettoMossa mossa_out;
        memset(&mossa_out, 0, sizeof(mossa_out));
        mossa_out.tipo_messaggio = MSG_MOSSA;
        mossa_out.direzione = dir;
        if (send_all(sock, &mossa_out, sizeof(mossa_out)) < 0) break;

        printf("\n[!] Mossa inviata. Attendo gli altri giocatori...\n");
    }

    printf("\nConnessione chiusa.\n");
    close(sock);
    return 0;
}
