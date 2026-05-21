# Dungeon Explorer

Progetto per il corso di **Laboratori di Sistemi Operativi** (Corso di Laurea in
Informatica, Università degli Studi di Napoli Federico II — A.A. 2025/2026).

Server multi-client in C che permette a 2-4 giocatori di esplorare in modo
cooperativo un dungeon generato casualmente. Il server gestisce le sessioni
di gioco in parallelo tramite thread POSIX e socket TCP/IP.

**Autori:** Simone Ascione (N86005160) — Vincenzo Aiello (N86004968)

---

# Requisiti

* Sistema operativo **Linux** o **WSL**.
* Una delle due toolchain seguenti:
  * **`gcc`** + **`make`** per la build nativa.
  * **Docker** + **Docker Compose v2** per la build containerizzata.

---

# Esecuzione con Docker Compose

# 1. Costruisci l'immagine
```bash
docker compose build
```

# 2. Avvia il server (in un terminale)
```bash
docker compose up dungeon_server
```
Il server si mette in ascolto sulla porta `8080` ed espone il log dei dungeon
creati e dei turni eseguiti. Per metterlo in background:
```bash
docker compose up -d dungeon_server
```

# 3. Avvia uno o più client (in terminali separati)
Ogni partita richiede da 2 a 4 giocatori. Apri un terminale per ognuno e lancia:
```bash
docker compose run --rm dungeon_client
```
Il flag `--rm` rimuove il container alla chiusura e permette di lanciarne più
istanze in parallelo.

# 4. Termina tutto
```bash
docker compose down
```

---

# Esecuzione locale (senza Docker)

# Compilazione
Dalla radice del progetto:
```bash
make
```
Vengono prodotti i due eseguibili:
* `server/server_app`
* `client/client_app`

# Avvio
In un terminale:
```bash
./server/server_app
```
In ognuno degli altri terminali (uno per giocatore):
```bash
./client/client_app
```
### Pulizia
```bash
make clean
```

---

# Come si gioca

1. **Avvio**: ogni client mostra il menu di lobby.
   * Scegliere `1` per **creare** un nuovo dungeon: il giocatore diventa
     *proprietario* della sessione.
   * Scegliere `2` per **entrare** in un dungeon esistente: occorre
     conoscerne l'ID (mostrato nel terminale del proprietario al momento
     della creazione).

2. **Fase di lobby**: quando un nuovo giocatore richiede l'ingresso, il
   proprietario riceve un prompt e decide se **accettare (1)** o
   **rifiutare (2)**. Raggiunto il numero minimo (2 giocatori), dopo ogni
   accettazione il proprietario può anche scegliere di **avviare la partita
   (1)** o **continuare ad accettare richieste (2)**.

3. **Partita**: ogni turno tutti i giocatori scelgono una direzione di
   movimento (`1` Nord, `2` Sud, `3` Est, `4` Ovest). Il server applica
   tutte le mosse contemporaneamente solo dopo aver ricevuto la scelta di
   ognuno. Le stanze possono contenere:
   * **Tesori** (`T`) — recuperano 15 HP.
   * **Mostri** (`M`) — infliggono 20 HP di danno.
   * **Trappole** (`X`) — infliggono 10 HP di danno.
   * **Stanze vuote** (`.`) — neutre.
   * **Uscita** (`U`) — se raggiunta, conclude la partita con la vittoria
     dell'intero gruppo.

4. **Fine partita**: il proprietario riceve un prompt finale e decide se
   **rigiocare (1)** con la stessa squadra (il server rigenera un nuovo
   dungeon mantenendo i partecipanti) oppure **sciogliere (2)** la sessione
   e disconnettere tutti.