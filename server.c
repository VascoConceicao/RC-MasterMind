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

int fd_udp, errcode;
ssize_t n;
socklen_t addrlen;
struct addrinfo hints_udp, *res_udp;
struct sockaddr_in addr;

char get_color(const char *str) {
    // Ensure the string is valid and contains exactly one character
    if (!str || str[1] != '\0') {
        return '\0'; // Return '\0' if the string is invalid or not size 1
    }

    char c = str[0]; // Extract the single character
    const char valid_chars[] = {'R', 'G', 'B', 'Y', 'O', 'P'};
    int size = sizeof(valid_chars) / sizeof(valid_chars[0]);
    
    for (int i = 0; i < size; i++) {
        if (c == valid_chars[i]) {
            return c; // Return the character if it is found in the list
        }
    }
    return '\0'; // Return '\0' if the character is not in the list
}

int get_integer(const char *str, size_t n) {
    if (strlen(str) != n)
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
    char **tries; // List of strings for tries
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

void init_game_tries(Game *game) {
    game->tries = NULL;
    game->nT = 0;
}

void add_try(Game *game, const char *new_try) {
    if (strlen(new_try) != 4) {
        fprintf(stderr, "Try must be a string of 4 characters.\n");
        return;
    }

    game->tries = realloc(game->tries, (game->nT + 1) * sizeof(char *));
    if (game->tries == NULL) {
        perror("Failed to allocate memory for tries");
        exit(EXIT_FAILURE);
    }

    game->tries[game->nT] = malloc(5 * sizeof(char)); // Allocate space for the string
    if (game->tries[game->nT] == NULL) {
        perror("Failed to allocate memory for a try");
        exit(EXIT_FAILURE);
    }

    strcpy(game->tries[game->nT], new_try);
    game->nT++;
}

void free_game_tries(Game *game) {
    for (int i = 0; i < game->nT; i++) {
        free(game->tries[i]);
    }
    free(game->tries);
    game->tries = NULL;
    game->nT = 0;
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

void add_game(GameArray *array, int PLID, int time_) {
    char key[5] = "0000";
    // Red, Green, Blue, Yellow, Orange, Purple
    const char colors[] = {'R', 'G', 'B', 'Y', 'O', 'P'};
    srand(time(NULL));
    for (int i = 0; i < 4; i++)
        key[i] = colors[rand() % 6];
    key[4] = '\0';

    Game new_game = {PLID, "", 1, NULL, time(NULL), time_};
    strncpy(new_game.key, key, sizeof(new_game.key));
    init_game_tries(&new_game);
    append_game(array, new_game);
}

void remove_game(GameArray *array, size_t index) {
    if (index >= array->size)
        return;

    free_game_tries(&array->games[index]); // Free the memory for tries

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

int get_game_index(int PLID, GameArray *array) {
    for (size_t i = 0; i < array->size; i++)
        if (array->games[i].PLID == PLID)
            return i;
    return -1;
}

void free_game_array(GameArray *array) {
    for (size_t i = 0; i < array->size; i++) {
        free_game_tries(&array->games[i]); // Free the memory for tries
    }
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
        printf("  Max Playtime = %d seconds\n", array->games[i].max_playtime);
        printf("  Tries:\n");
        for (int j = 0; j < array->games[i].nT; j++) {
            printf("    Try %d: %s\n", j + 1, array->games[i].tries[j]);
        }
        printf("\n");
    }
}

int is_correct_guess(GameArray *array, int PLID, char iC1, char iC2, char iC3, char iC4) {
    int game_index = get_game_index(PLID, array);
    char key[5];
    sprintf(key, "%s", array -> games[game_index].key);
    return key[0] == iC1 && key[1] == iC2 && key[2] == iC3 && key[3] == iC4;
}

int is_repeated_try(GameArray *array, int PLID, char iC1, char iC2, char iC3, char iC4) {

}

int is_resend(GameArray *array, int PLID, int nT) {

}

void calculateBlackAndWhite(const char* secret, const char* guess, int* nB, int* nW) {
    int length = strlen(secret);
    int black = 0, white = 0;

    // Arrays to track counts of unmatched characters
    int secretCount[256] = {0};
    int guessCount[256] = {0};

    // First pass: count black matches
    for (int i = 0; i < length; i++) {
        if (secret[i] == guess[i]) {
            black++;
        } else {
            // Track unmatched characters
            secretCount[(unsigned char)secret[i]]++;
            guessCount[(unsigned char)guess[i]]++;
        }
    }

    // Second pass: count white matches
    for (int i = 0; i < 256; i++) {
        white += (secretCount[i] < guessCount[i]) ? secretCount[i] : guessCount[i];
    }

    // Assign the results
    *nB = black;
    *nW = white;
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

    fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == -1) {
        exit(1);
    }

    memset(&hints_udp, 0, sizeof hints_udp);
    hints_udp.ai_family = AF_INET;
    hints_udp.ai_socktype = SOCK_DGRAM;
    /* É passada uma flag para indicar que o socket é passivo.
    Esta flag é usada mais tarde pela função `bind()` e indica que
    o socket aceita conceções. */
    hints_udp.ai_flags = AI_PASSIVE;

    /* Ao passar o endereço `NULL`, indicamos que somos nós o Host. */
    errcode = getaddrinfo(NULL, GSport, &hints_udp, &res_udp);
    if (errcode != 0) {
        exit(1);
    }

    /* Quando uma socket é criada, não tem um endereço associado.
    Esta função serve para associar um endereço à socket, de forma a ser acessível
    por conexões externas ao programa.
    É associado o nosso endereço (`res->ai_addr`, definido na chamada à função `getaddrinfo()`).*/
    n = bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen);
    if (n == -1) {
        exit(1);
    }

    int max_size = 512;
    char buffer[max_size], PLID[max_size], status[max_size];
    GameArray games_array;
    init_game(&games_array);

    /* Loop para receber bytes e processá-los */
    while (1) {
        addrlen = sizeof(addr);
        /* Lê da socket (fd) 128 bytes e guarda-os no buffer.
        Existem flags opcionais que não são passadas (0).
        O endereço do cliente (e o seu tamanho) são guardados para mais tarde devolver o texto */
        n = recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr *)&addr, &addrlen);
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
            case SNG: {
                char max_playtime[max_size];
                sscanf(args, "%s %s", PLID, max_playtime);
                int iPLID = get_integer(PLID, 6), iTime = get_time(max_playtime), game_index = get_game_index(iPLID, &games_array);

                if (iPLID == -1 || iTime == -1) {
                    sprintf(status, "%s", "ERR");
                } else if (get_game_index(iPLID, &games_array) != -1) {
                    if (has_x_seconds_passed(games_array.games[game_index].start_time, games_array.games[game_index].max_playtime)) {
                        remove_game(&games_array, game_index);
                        add_game(&games_array, iPLID, iTime);
                        sprintf(status, "%s", "OK");
                    }
                    else
                        sprintf(status, "%s", "NOK");
                } else {
                    add_game(&games_array, iPLID, iTime);
                    sprintf(status, "%s", "OK");
                }
                sprintf(buffer, "RSG %s\n", status);
                break;
            }
            
            case TRY: {
                char C1[max_size], C2[max_size], C3[max_size], C4[max_size], nT[max_size];
                sscanf(args, "%s %s %s %s %s %s", PLID, C1, C2, C3, C4, nT);
                int iPLID = get_integer(PLID, 6), inT = get_integer(nT, 1), game_index = get_game_index(iPLID, &games_array);
                char iC1 = get_color(C1), iC2 = get_color(C2), iC3 = get_color(C3), iC4 = get_color(C4);
                printf("Try: %d, %d, %c, %c, %c, %c\n", iPLID, inT, iC1, iC2, iC3, iC4);
                printf("Correct guess? %d\n", is_correct_guess(&games_array, iPLID, iC1, iC2, iC3, iC4));
                if (iPLID == -1 || inT == -1 || iC1 == '\0' || iC2 == '\0' || iC3 == '\0' || iC4 == '\0') {
                    sprintf(status, "ERR");
                } else if (has_x_seconds_passed(games_array.games[game_index].start_time, games_array.games[game_index].max_playtime)) {
                    char key[5];
                    sprintf(key, "%s", games_array.games[game_index].key);
                    remove_game(&games_array, game_index);
                    sprintf(status, "ETM %c %c %c %c", key[0], key[1], key[2], key[3]);
                } else if (inT == 8 && !is_correct_guess(&games_array, iPLID, iC1, iC2, iC3, iC4)) {
                    char key[5];
                    sprintf(key, "%s", games_array.games[game_index].key);
                    remove_game(&games_array, game_index);
                    sprintf(status, "ENT %c %c %c %c", key[0], key[1], key[2], key[3]);
                } else if (is_repeated_try(&games_array, iPLID, iC1, iC2, iC3, iC4) && !is_resend(&games_array, iPLID, inT)) {
                    sprintf(status, "DUP");
                } else {
                    char key[5];
                    sprintf(key, "%s", games_array.games[game_index].key);
                    char try[5];
                    sprintf(try, "%c%c%c%c", iC1, iC2, iC3, iC4);
                    int nB = 0, nW = 0;
                    calculateBlackAndWhite(key, try, &nB, &nW);
                    add_try(&games_array.games[game_index], try);
                    sprintf(status, "OK %d %d %d", inT, nB, nW);
                }
                // A PARTIR DAQUI NAO ESTA FEITO
                sprintf(buffer, "RTR %s\n", status);
                break;
            }
        }

        printf("message sent: %s\n", buffer);
        print_games(&games_array);
        /* Envia a mensagem recebida (atualmente presente no buffer) para o endereço `addr` de onde foram recebidos dados */
        n = sendto(fd_udp, buffer, strlen(buffer), 0, (struct sockaddr *)&addr, addrlen);
        if (n == -1) {
            exit(1);
        }
    }

    free_game_array(&games_array);
    freeaddrinfo(res_udp);
    close(fd_udp);
}