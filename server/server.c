#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include "protocol.h"

#define MAX_SESSIONI 16

typedef enum {
    SESS_LOBBY        = 0,
    SESS_IN_GIOCO     = 1,
    SESS_POST_PARTITA = 2,   
    SESS_FINITA       = 3
} StatoSessione;

typedef struct {
    int in_uso;                              
    Dungeon dungeon;                         
    int owner_sock;                          
    int sock_giocatori[MAX_PLAYERS];         
    int mosse_ricevute;
    int turno_round;                         
    StatoSessione stato;                     

    
    int joiner_sock_pendente;                
    int decisione_owner;                     
    int pid_assegnato;                       
    int sock_decisione_per;                  

    pthread_mutex_t lock;
    pthread_cond_t  cond_turno;              
    pthread_cond_t  cond_owner;              
    pthread_cond_t  cond_pending_risolto;    
    pthread_cond_t  cond_pending_libero;     
    pthread_cond_t  cond_partita_iniziata;   
    pthread_cond_t  cond_post_partita;       
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
    s->stato = SESS_LOBBY;
    s->joiner_sock_pendente = -1;
    s->decisione_owner = 0;
    s->pid_assegnato = -1;
    s->sock_decisione_per = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) s->sock_giocatori[i] = -1;
    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->cond_turno, NULL);
    pthread_cond_init(&s->cond_owner, NULL);
    pthread_cond_init(&s->cond_pending_risolto, NULL);
    pthread_cond_init(&s->cond_pending_libero, NULL);
    pthread_cond_init(&s->cond_partita_iniziata, NULL);
    pthread_cond_init(&s->cond_post_partita, NULL);
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

static int conta_attivi(Sessione *s) {
    int n = 0;
    for (int i = 0; i < s->dungeon.num_eroi; i++) {
        if (s->dungeon.eroi[i].attivo) n++;
    }
    return n;
}

static void esegui_turno_e_broadcast(Sessione *s) {
    int vivi_rimasti = 0;
    for (int i = 0; i < s->dungeon.num_eroi; i++) {
        Giocatore *g = &s->dungeon.eroi[i];
        if (g->attivo && g->mossa_pronta) {
            muovi_giocatore(&s->dungeon, i, g->direzione_scelta);
        }
        g->mossa_pronta = 0;
        if (g->attivo && g->hp > 0) vivi_rimasti++;
    }
    if (vivi_rimasti == 0 && s->dungeon.partita_finita != 1) {
        s->dungeon.partita_finita = -1;
    }

    s->mosse_ricevute = 0;
    s->turno_round++;

    const char *log = "Turno concluso! Cosa fai adesso?";
    if (s->dungeon.partita_finita == 1)       log = "L'USCITA E' STATA TROVATA! VITTORIA!";
    else if (s->dungeon.partita_finita == -1) log = "IL GRUPPO E' STATO ANNIENTATO...";

    trasmetti_stato(s, log);
    pthread_cond_broadcast(&s->cond_turno);
}

static int gestisci_lobby_owner(Sessione *s, int owner_sock) {
    while (1) {
        pthread_mutex_lock(&s->lock);
        while (s->joiner_sock_pendente == -1 && s->stato == SESS_LOBBY) {
            pthread_cond_wait(&s->cond_owner, &s->lock);
        }
        if (s->stato != SESS_LOBBY) {
            pthread_mutex_unlock(&s->lock);
            return -1;
        }
        pthread_mutex_unlock(&s->lock);

        
        PacchettoLobby prompt;
        memset(&prompt, 0, sizeof(prompt));
        prompt.tipo_messaggio = MSG_OWNER_PROMPT_RICHIESTA;
        snprintf(prompt.payload, sizeof(prompt.payload),
                 "Un giocatore vuole entrare nel dungeon. Accetti? (1=Si, 2=No)");
        if (send_all(owner_sock, &prompt, sizeof(prompt)) < 0) return -1;

        PacchettoLobby risp;
        if (recv_all(owner_sock, &risp, sizeof(risp)) < 0) return -1;

        pthread_mutex_lock(&s->lock);
        
        int sock_pending_corrente = s->joiner_sock_pendente;
        if (risp.tipo_messaggio == MSG_OWNER_ACCETTA && sock_pending_corrente >= 0) {
            int pid = aggiungi_player_alla_sessione(s, sock_pending_corrente);
            if (pid >= 0) {
                s->decisione_owner = 1;
                s->pid_assegnato = pid;
            } else {
                
                s->decisione_owner = -1;
                s->pid_assegnato = -1;
            }
        } else {
            s->decisione_owner = -1;
            s->pid_assegnato = -1;
        }
        s->sock_decisione_per = sock_pending_corrente;   
        s->joiner_sock_pendente = -1;
        pthread_cond_broadcast(&s->cond_pending_risolto);
        pthread_cond_broadcast(&s->cond_pending_libero);

        int num = s->dungeon.num_eroi;
        int decisione_finale = s->decisione_owner;
        pthread_mutex_unlock(&s->lock);

        printf("[LOBBY] Dungeon %d: owner ha %s. Giocatori: %d/%d.\n",
               s->dungeon.id,
               (decisione_finale == 1) ? "accettato" : "rifiutato",
               num, MAX_PLAYERS);

        
        if (num >= MIN_PLAYERS) {
            
            
            
            
            int dungeon_pieno = (num >= MAX_PLAYERS);

            PacchettoLobby prompt_start;
            memset(&prompt_start, 0, sizeof(prompt_start));
            prompt_start.tipo_messaggio = MSG_OWNER_PROMPT_START;
            if (dungeon_pieno) {
                snprintf(prompt_start.payload, sizeof(prompt_start.payload),
                         "Dungeon al completo (%d/%d). Premi 1 per avviare la partita.",
                         num, MAX_PLAYERS);
            } else {
                snprintf(prompt_start.payload, sizeof(prompt_start.payload),
                         "Hai %d giocatori. Avvia partita ora? (1=Si, 2=Continua ad accettare)",
                         num);
            }
            if (send_all(owner_sock, &prompt_start, sizeof(prompt_start)) < 0) return -1;

            PacchettoLobby risp_start;
            if (recv_all(owner_sock, &risp_start, sizeof(risp_start)) < 0) return -1;

            int avvia = (risp_start.tipo_messaggio == MSG_OWNER_START) || dungeon_pieno;

            if (avvia) {
                pthread_mutex_lock(&s->lock);
                s->stato = SESS_IN_GIOCO;
                
                
                
                
                pthread_cond_broadcast(&s->cond_partita_iniziata);
                pthread_cond_broadcast(&s->cond_pending_risolto);
                pthread_cond_broadcast(&s->cond_pending_libero);
                pthread_mutex_unlock(&s->lock);

                PacchettoLobby start_msg;
                memset(&start_msg, 0, sizeof(start_msg));
                start_msg.tipo_messaggio = MSG_LOBBY_GAME_START;
                snprintf(start_msg.payload, sizeof(start_msg.payload),
                         "Partita avviata con %d eroi!", num);
                if (send_all(owner_sock, &start_msg, sizeof(start_msg)) < 0) return -1;
                return 0;
            }
        }
    }
}

static int gestisci_lobby_joiner(Sessione *s, int joiner_sock, int *out_pid) {
    PacchettoLobby risposta;
    memset(&risposta, 0, sizeof(risposta));

    pthread_mutex_lock(&s->lock);

    
    while (s->joiner_sock_pendente != -1 && s->stato == SESS_LOBBY) {
        pthread_cond_wait(&s->cond_pending_libero, &s->lock);
    }
    if (s->stato != SESS_LOBBY) {
        pthread_mutex_unlock(&s->lock);
        risposta.tipo_messaggio = MSG_LOBBY_ERRORE;
        strncpy(risposta.payload, "La partita e' gia' iniziata o terminata.",
                sizeof(risposta.payload) - 1);
        send_all(joiner_sock, &risposta, sizeof(risposta));
        return -1;
    }
    if (s->dungeon.num_eroi >= MAX_PLAYERS) {
        pthread_mutex_unlock(&s->lock);
        risposta.tipo_messaggio = MSG_LOBBY_ERRORE;
        strncpy(risposta.payload, "Il dungeon e' pieno.",
                sizeof(risposta.payload) - 1);
        send_all(joiner_sock, &risposta, sizeof(risposta));
        return -1;
    }

    s->joiner_sock_pendente = joiner_sock;
    pthread_cond_signal(&s->cond_owner);

    
    
    
    
    while (s->sock_decisione_per != joiner_sock && s->stato == SESS_LOBBY) {
        pthread_cond_wait(&s->cond_pending_risolto, &s->lock);
    }

    
    
    if (s->sock_decisione_per != joiner_sock) {
        if (s->joiner_sock_pendente == joiner_sock) {
            s->joiner_sock_pendente = -1;
            pthread_cond_broadcast(&s->cond_pending_libero);
        }
        pthread_mutex_unlock(&s->lock);

        risposta.tipo_messaggio = MSG_LOBBY_ERRORE;
        strncpy(risposta.payload, "La partita e' iniziata o terminata prima della tua accettazione.",
                sizeof(risposta.payload) - 1);
        send_all(joiner_sock, &risposta, sizeof(risposta));
        return -1;
    }

    int accettato = (s->decisione_owner == 1);
    int pid = s->pid_assegnato;
    
    
    s->sock_decisione_per = -1;

    pthread_mutex_unlock(&s->lock);

    if (!accettato) {
        risposta.tipo_messaggio = MSG_LOBBY_ERRORE;
        strncpy(risposta.payload, "Richiesta rifiutata dal proprietario.",
                sizeof(risposta.payload) - 1);
        send_all(joiner_sock, &risposta, sizeof(risposta));
        return -1;
    }

    
    risposta.tipo_messaggio = MSG_LOBBY_OK;
    risposta.dungeon_id = s->dungeon.id;
    risposta.mio_player_id = pid;
    snprintf(risposta.payload, sizeof(risposta.payload),
             "Sei stato accettato nel dungeon #%d come player %d. In attesa che l'owner avvii la partita...",
             s->dungeon.id, pid);
    if (send_all(joiner_sock, &risposta, sizeof(risposta)) < 0) return -1;

    pthread_mutex_lock(&s->lock);
    while (s->stato == SESS_LOBBY) {
        pthread_cond_wait(&s->cond_partita_iniziata, &s->lock);
    }
    int stato_finale = s->stato;
    pthread_mutex_unlock(&s->lock);

    if (stato_finale != SESS_IN_GIOCO) {
        
        PacchettoLobby fine;
        memset(&fine, 0, sizeof(fine));
        fine.tipo_messaggio = MSG_LOBBY_ERRORE;
        strncpy(fine.payload, "La partita e' stata annullata.",
                sizeof(fine.payload) - 1);
        send_all(joiner_sock, &fine, sizeof(fine));
        return -1;
    }

    PacchettoLobby start_msg;
    memset(&start_msg, 0, sizeof(start_msg));
    start_msg.tipo_messaggio = MSG_LOBBY_GAME_START;
    strncpy(start_msg.payload, "L'owner ha avviato la partita!",
            sizeof(start_msg.payload) - 1);
    if (send_all(joiner_sock, &start_msg, sizeof(start_msg)) < 0) return -1;

    *out_pid = pid;
    return 0;
}

static int loop_di_gioco(Sessione *s, int sock, int mio_pid) {
    
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

        
        
        if (!s->dungeon.eroi[mio_pid].attivo) {
            pthread_mutex_unlock(&s->lock);
            continue;
        }

        int mio_round = s->turno_round;
        s->dungeon.eroi[mio_pid].direzione_scelta = mossa_in.direzione;
        s->dungeon.eroi[mio_pid].mossa_pronta = 1;
        s->mosse_ricevute++;

        printf("[SERVER] Dungeon %d: player %d pronto. (%d/%d attivi)\n",
               s->dungeon.id, mio_pid, s->mosse_ricevute, conta_attivi(s));

        
        
        
        
        while (s->turno_round == mio_round
               && s->mosse_ricevute < conta_attivi(s)
               && s->dungeon.partita_finita == 0) {
            pthread_cond_wait(&s->cond_turno, &s->lock);
        }

        
        
        
        if (s->turno_round == mio_round && s->dungeon.partita_finita == 0) {
            esegui_turno_e_broadcast(s);
        }

        if (s->dungeon.partita_finita != 0) {
            if (s->stato == SESS_IN_GIOCO) s->stato = SESS_POST_PARTITA;
            pthread_mutex_unlock(&s->lock);
            return 0;   
        }
        pthread_mutex_unlock(&s->lock);
    }
    return -1;   
}

static void resetta_dungeon_per_rigioco(Sessione *s) {
    int n_old = s->dungeon.num_eroi;
    int sock_temp[MAX_PLAYERS];
    for (int i = 0; i < MAX_PLAYERS; i++) {
        sock_temp[i] = (i < n_old) ? s->sock_giocatori[i] : -1;
    }

    int old_id = s->dungeon.id;

    memset(&s->dungeon, 0, sizeof(s->dungeon));
    s->dungeon.id = old_id;
    genera_dungeon(&s->dungeon);   

    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        s->sock_giocatori[i] = sock_temp[i];
        if (sock_temp[i] >= 0) {
            aggiungi_giocatore(&s->dungeon, i);   
        }
    }
    s->mosse_ricevute = 0;
    printf("[POST] Dungeon %d rigenerato per rigioco. Giocatori: %d.\n",
           s->dungeon.id, s->dungeon.num_eroi);
    stampa_dungeon_debug(&s->dungeon);
}

static int gestisci_post_partita(Sessione *s, int sock, int sono_owner) {
    if (sono_owner) {
        PacchettoLobby prompt;
        memset(&prompt, 0, sizeof(prompt));
        prompt.tipo_messaggio = MSG_OWNER_PROMPT_FINE;
        snprintf(prompt.payload, sizeof(prompt.payload),
                 "Partita conclusa. Cosa fai? (1=Rigioca con la stessa squadra, 2=Sciogli il gruppo)");
        if (send_all(sock, &prompt, sizeof(prompt)) < 0) goto sciogli_forzato;

        PacchettoLobby risp;
        if (recv_all(sock, &risp, sizeof(risp)) < 0) goto sciogli_forzato;

        pthread_mutex_lock(&s->lock);
        if (risp.tipo_messaggio == MSG_OWNER_RIGIOCA) {
            resetta_dungeon_per_rigioco(s);
            s->stato = SESS_IN_GIOCO;
            pthread_cond_broadcast(&s->cond_post_partita);
            pthread_mutex_unlock(&s->lock);

            PacchettoLobby fine;
            memset(&fine, 0, sizeof(fine));
            fine.tipo_messaggio = MSG_FINE_RIGIOCA;
            strncpy(fine.payload, "Si rigioca! Nuovo dungeon generato.",
                    sizeof(fine.payload) - 1);
            if (send_all(sock, &fine, sizeof(fine)) < 0) return -1;
            return 0;
        } else {
            s->stato = SESS_FINITA;
            pthread_cond_broadcast(&s->cond_post_partita);
            pthread_mutex_unlock(&s->lock);

            PacchettoLobby fine;
            memset(&fine, 0, sizeof(fine));
            fine.tipo_messaggio = MSG_FINE_SCIOGLI;
            strncpy(fine.payload, "Hai sciolto il gruppo. Arrivederci!",
                    sizeof(fine.payload) - 1);
            send_all(sock, &fine, sizeof(fine));
            return -1;
        }

sciogli_forzato:
        
        pthread_mutex_lock(&s->lock);
        s->stato = SESS_FINITA;
        pthread_cond_broadcast(&s->cond_post_partita);
        pthread_mutex_unlock(&s->lock);
        return -1;
    }

    
    pthread_mutex_lock(&s->lock);
    while (s->stato == SESS_POST_PARTITA) {
        pthread_cond_wait(&s->cond_post_partita, &s->lock);
    }
    int stato_finale = s->stato;
    pthread_mutex_unlock(&s->lock);

    PacchettoLobby fine;
    memset(&fine, 0, sizeof(fine));
    if (stato_finale == SESS_IN_GIOCO) {
        fine.tipo_messaggio = MSG_FINE_RIGIOCA;
        strncpy(fine.payload, "L'owner ha avviato un nuovo dungeon!",
                sizeof(fine.payload) - 1);
        if (send_all(sock, &fine, sizeof(fine)) < 0) return -1;
        return 0;
    }
    fine.tipo_messaggio = MSG_FINE_SCIOGLI;
    strncpy(fine.payload, "L'owner ha sciolto il gruppo.",
            sizeof(fine.payload) - 1);
    send_all(sock, &fine, sizeof(fine));
    return -1;
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
    int sono_owner = 0;

    if (richiesta.tipo_messaggio == MSG_CREA_DUNGEON) {
        sono_owner = 1;
        idx_sessione = crea_sessione(sock);
        if (idx_sessione < 0) {
            PacchettoLobby err;
            memset(&err, 0, sizeof(err));
            err.tipo_messaggio = MSG_LOBBY_ERRORE;
            strncpy(err.payload, "Limite massimo di dungeon raggiunto.",
                    sizeof(err.payload) - 1);
            send_all(sock, &err, sizeof(err));
            close(sock);
            return NULL;
        }
        Sessione *s = &sessioni[idx_sessione];
        pthread_mutex_lock(&s->lock);
        mio_pid = aggiungi_player_alla_sessione(s, sock);
        pthread_mutex_unlock(&s->lock);

        PacchettoLobby ok;
        memset(&ok, 0, sizeof(ok));
        ok.tipo_messaggio = MSG_LOBBY_OK;
        ok.dungeon_id = s->dungeon.id;
        ok.mio_player_id = mio_pid;
        snprintf(ok.payload, sizeof(ok.payload),
                 "Dungeon #%d creato. Sei il PROPRIETARIO (player %d). Attendo richieste...",
                 s->dungeon.id, mio_pid);
        if (send_all(sock, &ok, sizeof(ok)) < 0) { close(sock); return NULL; }
    }
    else if (richiesta.tipo_messaggio == MSG_ENTRA_DUNGEON) {
        idx_sessione = trova_sessione(richiesta.dungeon_id);
        if (idx_sessione < 0) {
            PacchettoLobby err;
            memset(&err, 0, sizeof(err));
            err.tipo_messaggio = MSG_LOBBY_ERRORE;
            snprintf(err.payload, sizeof(err.payload),
                     "Dungeon #%d non trovato.", richiesta.dungeon_id);
            send_all(sock, &err, sizeof(err));
            close(sock);
            return NULL;
        }
        Sessione *s = &sessioni[idx_sessione];
        if (gestisci_lobby_joiner(s, sock, &mio_pid) < 0) {
            close(sock);
            return NULL;
        }
    }
    else {
        PacchettoLobby err;
        memset(&err, 0, sizeof(err));
        err.tipo_messaggio = MSG_LOBBY_ERRORE;
        strncpy(err.payload, "Comando di lobby non riconosciuto.",
                sizeof(err.payload) - 1);
        send_all(sock, &err, sizeof(err));
        close(sock);
        return NULL;
    }

    Sessione *s = &sessioni[idx_sessione];

    
    if (sono_owner) {
        if (gestisci_lobby_owner(s, sock) < 0) {
            close(sock);
            return NULL;
        }
    }

    while (1) {
        if (loop_di_gioco(s, sock, mio_pid) < 0) break;
        if (gestisci_post_partita(s, sock, sono_owner) != 0) break;
        
    }

    printf("[THREAD] Dungeon %d, player %d disconnesso.\n", s->dungeon.id, mio_pid);

    
    
    pthread_mutex_lock(&mutex_sessioni);
    pthread_mutex_lock(&s->lock);

    if (mio_pid >= 0 && mio_pid < MAX_PLAYERS) {
        s->sock_giocatori[mio_pid] = -1;
        s->dungeon.eroi[mio_pid].attivo = 0;
        s->dungeon.eroi[mio_pid].mossa_pronta = 0;
    }

    if (sock == s->owner_sock && (s->stato == SESS_LOBBY || s->stato == SESS_POST_PARTITA)) {
        s->stato = SESS_FINITA;
    }

    if (s->stato == SESS_IN_GIOCO
        && conta_attivi(s) > 0
        && s->mosse_ricevute >= conta_attivi(s)
        && s->dungeon.partita_finita == 0) {
        esegui_turno_e_broadcast(s);
        if (s->dungeon.partita_finita != 0) s->stato = SESS_POST_PARTITA;
    }

    pthread_cond_broadcast(&s->cond_turno);
    pthread_cond_broadcast(&s->cond_owner);
    pthread_cond_broadcast(&s->cond_pending_risolto);
    pthread_cond_broadcast(&s->cond_pending_libero);
    pthread_cond_broadcast(&s->cond_partita_iniziata);
    pthread_cond_broadcast(&s->cond_post_partita);

    int qualcuno_ancora_connesso = (s->joiner_sock_pendente >= 0);
    for (int i = 0; i < MAX_PLAYERS && !qualcuno_ancora_connesso; i++) {
        if (s->sock_giocatori[i] >= 0) qualcuno_ancora_connesso = 1;
    }

    if (!qualcuno_ancora_connesso) {
        int dungeon_id = s->dungeon.id;
        s->in_uso = 0;
        s->stato = SESS_FINITA;
        pthread_mutex_unlock(&s->lock);

        
        
        pthread_mutex_destroy(&s->lock);
        pthread_cond_destroy(&s->cond_turno);
        pthread_cond_destroy(&s->cond_owner);
        pthread_cond_destroy(&s->cond_pending_risolto);
        pthread_cond_destroy(&s->cond_pending_libero);
        pthread_cond_destroy(&s->cond_partita_iniziata);
        pthread_cond_destroy(&s->cond_post_partita);

        printf("[POST] Sessione (dungeon ID=%d) liberata. Slot disponibile.\n", dungeon_id);
    } else {
        pthread_mutex_unlock(&s->lock);
    }
    pthread_mutex_unlock(&mutex_sessioni);

    close(sock);
    return NULL;
}

int main(void) {
    setbuf(stdout, NULL);

    
    
    
    signal(SIGPIPE, SIG_IGN);

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
