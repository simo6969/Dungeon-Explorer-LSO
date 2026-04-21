CC = gcc
CFLAGS = -Wall -Wextra -I./common -pthread

all: server_app client_app

server_app: server/server.c server/game_logic.c
	$(CC) $(CFLAGS) -o server/server_app server/server.c server/game_logic.c

client_app: client/client.c
	$(CC) $(CFLAGS) -o client/client_app client/client.c

clean:
	rm -f server/server_app client/client_app