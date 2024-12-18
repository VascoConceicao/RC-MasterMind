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
#include <ctype.h>

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

int get_PLID(const char *str) {
    if (strlen(str) != 6)
        return -1;

    for (int i = 0; str[i] != '\0'; i++)
        if (!isdigit(str[i]))
            return -1;

    return atoi(str);
}

int get_time(const char *str) {
    if (str == NULL || strlen(str) == 0)
        return -1;

    for (int i = 0; str[i] != '\0'; i++)
        if (!isdigit(str[i]))
            return -1;

    int number = atoi(str);
    if (number >= 1 && number <= 600)
        return number;
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
    char key[5];
    int nT;
    time_t start_time;
    int max_playtime;
} Game;

typedef struct {
    Game *games;
    size_t size;
    size_t capacity;
} GameArray;

void init_game(GameArray *array) {
    array->games = NULL;
    array->size = 0;
    array->capacity = 0;
}

void append_game(GameArray *array, Game new_game) {
    if (array->size == array->capacity) {
        array->capacity = (array->capacity == 0) ? 1 : array->capacity * 2;
        array->games = realloc(array->games, array->capacity * sizeof(Game));
        if (array->games == NULL)
            exit(EXIT_FAILURE);
    }

    array->games[array->size] = new_game;
    array->size++;
}

void remove_game(GameArray *array, size_t index) {
    if (index >= array->size)
        return;

    for (size_t i = index; i < array->size - 1; i++)
        array->games[i] = array->games[i + 1];

    array->size--;

    if (array->size < array->capacity / 4 && array->capacity > 1) {
        array->capacity /= 2;
        array->games = realloc(array->games, array->capacity * sizeof(Game));
        if (array->games == NULL)
            perror("Failed to reallocate memory");
    }
}

int get_game_index(int PLID, GameArray *games) {
    for (int i = 0; i < games->size; i++)
        if (games->games[i].PLID == PLID)
            return i;
    return -1;
}

void free_game_array(GameArray *array) {
    free(array->games);
    array->games = NULL;
    array->size = 0;
    array->capacity = 0;
}

void print_games(const GameArray *array) {
    if (array->size == 0) {
        printf("No games available.\n");
        return;
    }

    printf("Games in the array:\n");
    for (size_t i = 0; i < array->size; i++) {
        printf("Game %zu:\n", i);
        printf("  PLID         = %d\n", array->games[i].PLID);
        printf("  Key          = %s\n", array->games[i].key);
        printf("  nT           = %d\n", array->games[i].nT);
        printf("  Start Time   = %s", ctime(&array->games[i].start_time));
        printf("  Max Playtime = %d seconds\n\n", array->games[i].max_playtime);
    }
}

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
    char buffer[max_size], status[4];
    GameArray games;
    init_game(&games);

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
            char message[max_size];
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
                int iPLID = get_PLID(PLID), iTime = get_time(max_playtime);

                if (iPLID == -1 || iTime == -1) {
                    sprintf(status, "%s", "ERR");
                } else if (get_game_index(iPLID, &games) != -1) {
                    sprintf(status, "%s", "NOK");
                } else {
                    char key[5] = "0000";
                    // Red, Green, Blue, Yellow, Orange, Purple
                    const char colors[] = {'R', 'G', 'B', 'Y', 'O', 'P'};
                    srand(time(NULL));
                    for (int i = 0; i < 4; i++)
                        key[i] = colors[rand() % 6];
                    key[4] = '\0';

                    sprintf(status, "%s", "OK");
                    Game new_game = {iPLID, "", 1, time(NULL), iTime};
                    strncpy(new_game.key, key, sizeof(new_game.key));
                    append_game(&games, new_game);
                }
                sprintf(buffer, "RSG %s\n", status);
        }

        printf("message sent: %s\n", buffer);
        // print_games(&games);
        /* Envia a mensagem recebida (atualmente presente no buffer) para o endereço `addr` de onde foram recebidos dados */
        n = sendto(fd, buffer, n, 0, (struct sockaddr *)&addr, addrlen);
        if (n == -1) {
            exit(1);
        }
    }

    free_game_array(&games);
    freeaddrinfo(res);
    close(fd);
}