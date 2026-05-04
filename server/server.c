#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "protocol.h"

#define MAX_SESSIONI 16

typedef struct {
    int in_uso;                              
    Dungeon dungeon;                         
    int owner_sock;                          
    int sock_giocatori[MAX_PLAYERS];        
    int mosse_ricevute;
    pthread_mutex_t lock;
    pthread_cond_t  cond_turno;
} Sessione;

static Sessione sessioni[MAX_SESSIONI];
static pthread_mutex_t mutex_sessioni = PTHREAD_MUTEX_INITIALIZER;
static int prossimo_dungeon_id = 1;

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

static int crea_sessione(int owner_sock) {
    pthread_mutex_lock(&mutex_sessioni);
    int idx = -1;
    for (int i = 0; i < MAX_SESSIONI; i++) {
        if (!sessioni[i].in_uso) { idx = i; break; }
    }
    if (idx < 0) {
        pthread_mutex_unlock(&mutex_sessioni);
        return -1;
    }
    Sessione *s = &sessioni[idx];
    memset(s, 0, sizeof(*s));
    s->in_uso = 1;
    s->owner_sock = owner_sock;
    for (int i = 0; i < MAX_PLAYERS; i++) s->sock_giocatori[i] = -1;
    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->cond_turno, NULL);
    s->dungeon.id = prossimo_dungeon_id++;
    genera_dungeon(&s->dungeon);
    printf("[LOBBY] Creata sessione, dungeon ID=%d, owner_sock=%d\n",
           s->dungeon.id, owner_sock);
    stampa_dungeon_debug(&s->dungeon);
    pthread_mutex_unlock(&mutex_sessioni);
    return idx;
}

static int trova_sessione(int dungeon_id) {
    int idx = -1;
    pthread_mutex_lock(&mutex_sessioni);
    for (int i = 0; i < MAX_SESSIONI; i++) {
        if (sessioni[i].in_uso && sessioni[i].dungeon.id == dungeon_id) {
            idx = i;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_sessioni);
    return idx;
}

static int aggiungi_player_alla_sessione(Sessione *s, int sock) {
    if (s->dungeon.num_eroi >= MAX_PLAYERS) return -1;
    int pid = s->dungeon.num_eroi;            
    s->sock_giocatori[pid] = sock;
    aggiungi_giocatore(&s->dungeon, pid);
    return pid;
}

static void trasmetti_stato(Sessione *s, const char *log) {
    for (int i = 0; i < s->dungeon.num_eroi; i++) {
        int sock = s->sock_giocatori[i];
        if (sock < 0) continue;
        PacchettoStato out;
        memset(&out, 0, sizeof(out));
        out.tipo_messaggio = MSG_STATO;
        out.dungeon_id = s->dungeon.id;
        out.mio_player_id = i;
        out.mappa = s->dungeon;
        strncpy(out.log_eventi, log, sizeof(out.log_eventi) - 1);
        send_all(sock, &out, sizeof(out));
    }
}


void *gestisci_giocatore(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    PacchettoLobby richiesta;
    if (recv_all(sock, &richiesta, sizeof(richiesta)) < 0) {
        close(sock);
        return NULL;
    }

    int idx_sessione = -1;
    int mio_pid = -1;
    PacchettoLobby risposta;
    memset(&risposta, 0, sizeof(risposta));

    if (richiesta.tipo_messaggio == MSG_CREA_DUNGEON) {
        idx_sessione = crea_sessione(sock);
        if (idx_sessione < 0) {
            risposta.tipo_messaggio = MSG_LOBBY_ERRORE;
            strncpy(risposta.payload, "Limite massimo di dungeon raggiunto.",
                    sizeof(risposta.payload) - 1);
            send_all(sock, &risposta, sizeof(risposta));
            close(sock);
            return NULL;
        }
        Sessione *s = &sessioni[idx_sessione];
        pthread_mutex_lock(&s->lock);
        mio_pid = aggiungi_player_alla_sessione(s, sock);
        pthread_mutex_unlock(&s->lock);

        risposta.tipo_messaggio = MSG_LOBBY_OK;
        risposta.dungeon_id = s->dungeon.id;
        risposta.mio_player_id = mio_pid;
        snprintf(risposta.payload, sizeof(risposta.payload),
                 "Dungeon #%d creato. Sei il PROPRIETARIO (player %d).",
                 s->dungeon.id, mio_pid);
    }
    else if (richiesta.tipo_messaggio == MSG_ENTRA_DUNGEON) {
        idx_sessione = trova_sessione(richiesta.dungeon_id);
        if (idx_sessione < 0) {
            risposta.tipo_messaggio = MSG_LOBBY_ERRORE;
            snprintf(risposta.payload, sizeof(risposta.payload),
                     "Dungeon #%d non trovato.", richiesta.dungeon_id);
            send_all(sock, &risposta, sizeof(risposta));
            close(sock);
            return NULL;
        }
        Sessione *s = &sessioni[idx_sessione];
        pthread_mutex_lock(&s->lock);
        mio_pid = aggiungi_player_alla_sessione(s, sock);
        pthread_mutex_unlock(&s->lock);

        if (mio_pid < 0) {
            risposta.tipo_messaggio = MSG_LOBBY_ERRORE;
            strncpy(risposta.payload, "Dungeon pieno.",
                    sizeof(risposta.payload) - 1);
            send_all(sock, &risposta, sizeof(risposta));
            close(sock);
            return NULL;
        }
        risposta.tipo_messaggio = MSG_LOBBY_OK;
        risposta.dungeon_id = s->dungeon.id;
        risposta.mio_player_id = mio_pid;
        snprintf(risposta.payload, sizeof(risposta.payload),
                 "Entrato nel dungeon #%d come player %d.",
                 s->dungeon.id, mio_pid);
    }
    else {
        risposta.tipo_messaggio = MSG_LOBBY_ERRORE;
        strncpy(risposta.payload, "Comando di lobby non riconosciuto.",
                sizeof(risposta.payload) - 1);
        send_all(sock, &risposta, sizeof(risposta));
        close(sock);
        return NULL;
    }

    if (send_all(sock, &risposta, sizeof(risposta)) < 0) {
        close(sock);
        return NULL;
    }

    Sessione *s = &sessioni[idx_sessione];

    pthread_mutex_lock(&s->lock);
    {
        PacchettoStato primo;
        memset(&primo, 0, sizeof(primo));
        primo.tipo_messaggio = MSG_STATO;
        primo.dungeon_id = s->dungeon.id;
        primo.mio_player_id = mio_pid;
        primo.mappa = s->dungeon;
        snprintf(primo.log_eventi, sizeof(primo.log_eventi),
                 "Benvenuto, eroe %d! Dungeon #%d.", mio_pid, s->dungeon.id);
        send_all(sock, &primo, sizeof(primo));
    }
    pthread_mutex_unlock(&s->lock);

    PacchettoMossa mossa_in;
    while (recv_all(sock, &mossa_in, sizeof(mossa_in)) == 0) {
        pthread_mutex_lock(&s->lock);

        s->dungeon.eroi[mio_pid].direzione_scelta = mossa_in.direzione;
        s->dungeon.eroi[mio_pid].mossa_pronta = 1;
        s->mosse_ricevute++;

        printf("[SERVER] Dungeon %d: player %d pronto. (%d/%d)\n",
               s->dungeon.id, mio_pid, s->mosse_ricevute, s->dungeon.num_eroi);

        if (s->mosse_ricevute == s->dungeon.num_eroi) {
            int vivi_rimasti = 0;
            for (int i = 0; i < s->dungeon.num_eroi; i++) {
                if (s->dungeon.eroi[i].hp > 0) {
                    muovi_giocatore(&s->dungeon, i, s->dungeon.eroi[i].direzione_scelta);
                    s->dungeon.eroi[i].mossa_pronta = 0;
                    if (s->dungeon.eroi[i].hp > 0) vivi_rimasti++;
                }
            }
            if (vivi_rimasti == 0 && s->dungeon.partita_finita != 1) {
                s->dungeon.partita_finita = -1;
            }
            s->mosse_ricevute = 0;

            const char *log = "Turno concluso! Cosa fai adesso?";
            if (s->dungeon.partita_finita == 1)       log = "L'USCITA E' STATA TROVATA! VITTORIA!";
            else if (s->dungeon.partita_finita == -1) log = "IL GRUPPO E' STATO ANNIENTATO...";

            trasmetti_stato(s, log);
            pthread_cond_broadcast(&s->cond_turno);
        } else {
            pthread_cond_wait(&s->cond_turno, &s->lock);
        }

        if (s->dungeon.partita_finita != 0) {
            pthread_mutex_unlock(&s->lock);
            break;
        }
        pthread_mutex_unlock(&s->lock);
    }

    printf("[THREAD] Dungeon %d, player %d disconnesso.\n", s->dungeon.id, mio_pid);
    pthread_mutex_lock(&s->lock);
    s->sock_giocatori[mio_pid] = -1;
    pthread_mutex_unlock(&s->lock);
    close(sock);
    return NULL;
}

int main(void) {
    setbuf(stdout, NULL);
    srand((unsigned)time(NULL));   

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
    if (listen(server_socket, MAX_PLAYERS * MAX_SESSIONI) < 0) exit(1);

    printf("[SERVER-INFO] Server in ascolto sulla porta %d (max %d dungeon paralleli).\n",
           PORTA_SERVER, MAX_SESSIONI);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen);
        if (client_socket < 0) continue;

        int *new_sock = malloc(sizeof(int));
        if (!new_sock) { close(client_socket); continue; }
        *new_sock = client_socket;

        pthread_t tid;
        if (pthread_create(&tid, NULL, gestisci_giocatore, (void *)new_sock) == 0) {
            pthread_detach(tid);
        } else {
            free(new_sock);
            close(client_socket);
        }
    }
    return 0;
}
