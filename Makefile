CC = gcc                # Compiler
CFLAGS = -Wall -Wextra  # Compiler flags for warnings

# Default rule: build everything
all: server player

# Compile server
server: server.c
	$(CC) $(CFLAGS) -o server server.c

# Compile player
player: player.c
	$(CC) $(CFLAGS) -o player player.c