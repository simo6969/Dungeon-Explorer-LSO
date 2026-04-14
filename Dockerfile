# Usa l'immagine ufficiale di GCC (contiene già C e Make)
FROM gcc:latest

# Imposta la cartella di lavoro dentro il container
WORKDIR /app

# Copia tutti i file del progetto dentro il container
COPY . .

# Compila il codice quando il container viene costruito
RUN make server_app

# Esponi la porta 8080 (quella su cui il server ascolterà le connessioni)
EXPOSE 8080

# Il comando che il container eseguirà all'avvio: fa partire il server
CMD ["./server/server_app"]