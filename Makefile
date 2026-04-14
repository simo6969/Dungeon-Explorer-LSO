# Variabili
CC = gcc
CFLAGS = -Wall -Wextra -I./common -pthread

# Regola principale: compila tutto
all: server_app client_app

# Compila il Server
server_app: server/server.c
	$(CC) $(CFLAGS) -o server/server_app server/server.c

# Compila il Client
client_app: client/client.c
	$(CC) $(CFLAGS) -o client/client_app client/client.c

# Pulisce i file compilati (comando: make clean)
clean:
	rm -f server/server_app client/client_app