#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#define SNG 0
#define TRY 1
#define QUT 2
#define DBG 3
#define STR 4
#define SSB 5

int fd, errcode;
ssize_t n;
socklen_t addrlen;
struct addrinfo hints, *res;
struct sockaddr_in addr;

int getPLID(const char *str) {
    // Check if the string length is exactly 6
    if (strlen(str) != 6) {
        return -1;
    }

    // Check if all characters are digits
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) {
            return -1;
        }
    }

    return atoi(str); // The string is a 6-digit number
}

int getTime(const char *str) {
    // Check if the string is empty
    if (str == NULL || strlen(str) == 0) {
        return -1;
    }

    // Check if all characters are digits
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) {
            return -1; // Not a valid number
        }
    }

    // Convert the string to an integer
    int number = atoi(str);

    // Check if the number is in the range 1 to 600
    if (number >= 1 && number <= 600) {
        return number; // Valid number in range
    }

    return -1; // Out of range
}

int get_game_index(int PLID) {
    for (int i = 0; i < games_size; i++) {
        if (games[i].PLID == PLID)
            return i;
    }
    return -1;
}

int has_x_seconds_passed(time_t start_time, int x) {
    time_t current_time = time(NULL);
    if (current_time == ((time_t)-1))
        return -1;
    return difftime(current_time, start_time) >= x;
}

typedef struct {
    int PLID;
    char key[4];
    int nT;
    time_t start_time;
    int max_playtime;
} Game;

int main(int argc, char *argv[]) {

    char* GSport = "58080";
    int verbose = 0;

    int opt;

    while ((opt = getopt(argc, argv, "p:v")) != -1) {
        switch (opt) {
            case 'p':
                GSport = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-p GSport] [-v]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    /* É passada uma flag para indicar que o socket é passivo.
    Esta flag é usada mais tarde pela função `bind()` e indica que
    o socket aceita conceções. */
    hints.ai_flags = AI_PASSIVE;

    /* Ao passar o endereço `NULL`, indicamos que somos nós o Host. */
    errcode = getaddrinfo(NULL, GSport, &hints, &res);
    if (errcode != 0) {
        exit(1);
    }

    /* Quando uma socket é criada, não tem um endereço associado.
    Esta função serve para associar um endereço à socket, de forma a ser acessível
    por conexões externas ao programa.
    É associado o nosso endereço (`res->ai_addr`, definido na chamada à função `getaddrinfo()`).*/
    n = bind(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1) {
        exit(1);
    }

    int max_size = 512;
    char buffer[max_size], status[3];

    /* Loop para receber bytes e processá-los */
    while (1) {
        addrlen = sizeof(addr);
        /* Lê da socket (fd) 128 bytes e guarda-os no buffer.
        Existem flags opcionais que não são passadas (0).
        O endereço do cliente (e o seu tamanho) são guardados para mais tarde devolver o texto */
        n = recvfrom(fd, buffer, 128, 0, (struct sockaddr *)&addr, &addrlen);
        if (n == -1) {
            exit(1);
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(addr.sin_port);

        if (verbose) {
            char message[max_size]; // Adjust size as needed
            sprintf(message, "-------------------------------\nIP: %s\nPort: %d\n\n%s\n", client_ip, client_port, buffer);
            write(1, message, strlen(message));
        }

        char command_string[max_size], args[max_size];
        sscanf(buffer, "%s %[^\n]", command_string, args);
        int command;
        if (!strcmp(command_string, "SNG")) {
            command = SNG;
        } else if (!strcmp(command_string, "TRY")) {
            command = TRY;
        }
        
        switch (command) {
            case SNG:
                char PLID[max_size], max_playtime[max_size];
                sscanf(args, "%s %s", PLID, max_playtime);
                printf("PLID: %s\nmax_playtime: %s\n", PLID, max_playtime);
                int iPLID = getPLID(PLID), iTime = getTime(max_playtime);
                if (iPLID == -1 || iTime == -1) {
                    status = "ERR";
                } else if (get_game_index(PLID) == -1) {
                    status = "NOK";
                } else {
                    Game new_game;
                    new_game.PLID = PLID;
                    new_game.max_playtime = max_playtime;
                }

                sprintf(buffer, "RSG %s\n", status);
        }

        /* Envia a mensagem recebida (atualmente presente no buffer) para o endereço `addr` de onde foram recebidos dados */
        n = sendto(fd, buffer, n, 0, (struct sockaddr *)&addr, addrlen);
        if (n == -1) {
            exit(1);
        }
    }

    freeaddrinfo(res);
    close(fd);
}