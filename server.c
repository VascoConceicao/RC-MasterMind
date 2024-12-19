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

int fd_udp, fd_tcp, newfd_tcp, errcode, max_fd, max_size = 512;
ssize_t n_udp, n_tcp;
socklen_t addrlen;
struct addrinfo hints_udp, hints_tcp, *res_udp, *res_tcp;
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
    int *tries_time; // List of timestamps for each try
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
    game->tries_time = NULL;
    game->nT = 0;
}

void add_try(Game *game, const char *new_try, int timestamp) {
    if (strlen(new_try) != 4) {
        fprintf(stderr, "Try must be a string of 4 characters.\n");
        return;
    }

    game->tries = realloc(game->tries, (game->nT + 1) * sizeof(char *));
    game->tries_time = realloc(game->tries_time, (game->nT + 1) * sizeof(int));
    if (game->tries == NULL || game->tries_time == NULL) {
        perror("Failed to allocate memory for tries or tries_time");
        exit(EXIT_FAILURE);
    }

    game->tries[game->nT] = malloc(5 * sizeof(char));
    if (game->tries[game->nT] == NULL)
        exit(EXIT_FAILURE);

    strcpy(game->tries[game->nT], new_try);
    game->tries_time[game->nT] = timestamp;
    game->nT++;
}

void free_game_tries(Game *game) {
    for (int i = 0; i < game->nT; i++) {
        free(game->tries[i]);
    }
    free(game->tries);
    free(game->tries_time);
    game->tries = NULL;
    game->tries_time = NULL;
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

    Game new_game = {PLID, "", 1, NULL, NULL, time(NULL), time_};
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

void get_tries(const Game *game, char *buffer) {
    if (game->nT == 0) {
        sprintf(buffer, " ");
        return;
    }
    
    size_t current_length = 0;
    for (int i = 0; i < game->nT; i++) {
        char try_entry[max_size];
        sprintf(try_entry, "Trial: %c %c %c %c at %ds,", game->tries[i][0], game->tries[i][1], game->tries[i][2], game->tries[i][3], game->tries_time[i]);
        sprintf(buffer + current_length, "%s", try_entry);
        current_length += strlen(try_entry);
    }
    buffer[strlen(buffer) - 1] = '\0';
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

        char tries[max_size];
        get_tries(&array->games[i], tries);
        printf("  %s\n", tries);
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
    int game_index = get_game_index(PLID, array);
    char try[5];
    sprintf(try, "%c%c%c%c", iC1, iC2, iC3, iC4);
    int found = 0;
    for (int i = 0; i < array -> games[game_index].nT; i++) {
        if (!strcmp(try, array -> games[game_index].tries[i])) {
            found = 1;
            break;
        }
    }
    return found;
}

int is_resend(GameArray *array, int PLID, char iC1, char iC2, char iC3, char iC4, int inT) {
    int game_index = get_game_index(PLID, array);
    if (array -> games[game_index].nT == inT) {
        char try[5];
        sprintf(try, "%c%c%c%c", iC1, iC2, iC3, iC4);
        char previous_try[5];
        sprintf(previous_try, "%s", array -> games[game_index].tries[array -> games[game_index].nT - 1]);
        return !strcmp(try, previous_try);
    } else
        return 0;
}

int has_ongoing_game(GameArray *array, int PLID) {
    int game_index = get_game_index(PLID, array);

    if (game_index == -1) {
        return 0; // No game found for the given PLID
    }

    Game *game = &array->games[game_index];
    if (has_x_seconds_passed(game->start_time, game->max_playtime)) {
        // If the max playtime has passed, remove the game from the array
        remove_game(array, game_index);
        return 0; // No ongoing game
    }

    return 1; // The game is ongoing
}

int delta_expected_trial_number(GameArray *array, int PLID, int nT) {
    int game_index = get_game_index(PLID, array);
    return nT - array -> games[game_index].nT - 1;
}

int is_different_than_previous_try(GameArray *array, int PLID, char C1, char C2, char C3, char C4) {
    int game_index = get_game_index(PLID, array);
    char try[5];
    sprintf(try, "%c%c%c%c", C1, C2, C3, C4);
    char previous_try[5];
    sprintf(previous_try, "%s", array -> games[game_index].tries[array -> games[game_index].nT - 1]);
    return strcmp(try, previous_try);
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

void add_debug_game(GameArray *array, int PLID, int time_, char C1, char C2, char C3, char C4) {
    char key[5];
    sprintf(key, "%c%c%c%c", C1, C2, C3, C4);
    key[4] = '\0';

    Game new_game = {PLID, "", 1, NULL, NULL, time(NULL), time_};
    strncpy(new_game.key, key, sizeof(new_game.key));
    init_game_tries(&new_game);
    append_game(array, new_game);
}

void get_formatted_start_time(Game *game, char *formatted_time, size_t size) {
    struct tm *timeinfo;
    timeinfo = localtime(&game->start_time);
    strftime(formatted_time, size, "%Y-%m-%d %H:%M:%S", timeinfo);
}

int get_time_left(Game *game) {
    time_t current_time;
    time(&current_time);

    int elapsed_time = (int)difftime(current_time, game->start_time);
    int time_left = game->max_playtime - elapsed_time;

    return time_left > 0 ? time_left : 0;
}

int get_file(char *Fname, char *Fsize, char *Fdata, GameArray* array, int game_index) {

    int finalized = 0;
    char beggining[max_size], ending[max_size], mode[max_size], formatted_start_time[20];
    Game game = array->games[game_index];
    
    sprintf(Fname, "STATE_%d.txt", game.PLID);
    sprintf(Fsize, "SIZE TODO");
    get_formatted_start_time(&game, formatted_start_time, sizeof(formatted_start_time));
    if (has_x_seconds_passed(game.start_time, game.max_playtime)) {
        sprintf(beggining, "Last finalized game for player");
        sprintf(mode, "Mode: TODO Secret code: %s\n", game.key);
        sprintf(ending, "Termination: TODO, Duration: TODO\n");
        finalized = 1;
    } else {
        sprintf(beggining, "Active game found for player");
        sprintf(mode, "\n");
        sprintf(ending, "  -- %d seconds remaining to be completed --\n", get_time_left(&game));
    }

    sprintf(Fdata + strlen(Fdata), "     %s %d\nGame initiated: %s with %ds to be completed\n%s\n", beggining, game.PLID, formatted_start_time, game.max_playtime, mode);
    char tries[max_size];
    get_tries(&array->games[game_index], tries);
    if (!strcmp(tries, " "))
        sprintf(Fdata + strlen(Fdata), "     Game started - no transactions found     ");
    else
        sprintf(Fdata + strlen(Fdata), "     --- Transactions found: %d ---\n\n%s\n\n", game.nT, tries);
        
    sprintf(Fdata + strlen(Fdata), "%s", ending);
    
    return finalized;
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
    n_udp = bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen);
    if (n_udp == -1)
        exit(1);
    
    fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_tcp == -1)
        exit(1);

    opt = 1;
    if (setsockopt(fd_tcp, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        exit(1);

    memset(&hints_tcp, 0, sizeof hints_tcp);
    hints_tcp.ai_family = AF_INET;
    hints_tcp.ai_socktype = SOCK_STREAM;
    hints_tcp.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo(NULL, GSport, &hints_tcp, &res_tcp);
    if ((errcode) != 0)
        exit(1);

    n_tcp = bind(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen);
    if (n_tcp == -1)
        exit(1);
    
    /* Prepara para receber até 5 conexões na socket fd.
    Recusa outras conexões enquanto estiverem 5 conexões pendentes. */
    if (listen(fd_tcp, 5) == -1)
        exit(1);

    // Use select() to handle both sockets
    fd_set readfds;
    max_fd = (fd_udp > fd_tcp) ? fd_udp : fd_tcp;

    char buffer[max_size], PLID[max_size], status[max_size];
    GameArray games_array;
    init_game(&games_array);

    /* Loop para receber bytes e processá-los */
    while (1) {
        FD_ZERO(&readfds);        // Clear the set
        FD_SET(fd_udp, &readfds); // Add UDP socket to the set
        FD_SET(fd_tcp, &readfds); // Add TCP socket to the set

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0)
            exit(1);

        // Check for UDP activity
        int udp_or_tcp = 0;
        if (FD_ISSET(fd_udp, &readfds)) {
            addrlen = sizeof(addr);
            n_udp = recvfrom(fd_udp, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&addr, &addrlen);
            if (n_udp > 0) {
                buffer[n_udp] = '\0';
                udp_or_tcp = 0;
            }
        }

        // Check for TCP activity
        if (FD_ISSET(fd_tcp, &readfds)) {
            newfd_tcp = accept(fd_tcp, (struct sockaddr *)&addr, &addrlen);
            if (newfd_tcp == -1)
                continue;

            n_tcp = read(newfd_tcp, buffer, sizeof(buffer) - 1);
            if (n_tcp > 0) {
                buffer[n_tcp] = '\0';
                udp_or_tcp = 1;
            }
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(addr.sin_port);

        if (verbose) {
            char message[max_size];
            sprintf(message, "-------------------------------\nIP: %s\nPort: %d\nReceived: %s", client_ip, client_port, buffer);
            write(1, message, strlen(message));
        }

        char command_string[max_size], args[max_size];
        sscanf(buffer, "%s %[^\n]", command_string, args);
        int command;
        if (!strcmp(command_string, "SNG")) {
            command = SNG;
        } else if (!strcmp(command_string, "QUT")) {
            command = QUT;
        } else if (!strcmp(command_string, "TRY")) {
            command = TRY;
        } else if (!strcmp(command_string, "DBG")) {
            command = DBG;
        } else if (!strcmp(command_string, "STR")) {
            command = STR;
        } else if (!strcmp(command_string, "SSB")) {
            command = SSB;
        }else if (!strcmp(command_string, "STR")) {
            command = STR;
        } else if (!strcmp(command_string, "SSB")) {
            command = SSB;
        }
        
        switch (command) {
            case SNG: {
                char max_playtime[max_size];
                sscanf(args, "%s %s", PLID, max_playtime);
                int iPLID = get_integer(PLID, 6), iTime = get_time(max_playtime), game_index = get_game_index(iPLID, &games_array);

                if (iPLID == -1 || iTime == -1) {
                    sprintf(status, "%s", "ERR");
                } else if (game_index != -1) {
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

                if (iPLID == -1 || inT == -1 || iC1 == '\0' || iC2 == '\0' || iC3 == '\0' || iC4 == '\0') {
                    sprintf(status, "ERR");
                } else if (game_index == -1) {
                    sprintf(status, "NOK");
                } else if (has_x_seconds_passed(games_array.games[game_index].start_time, games_array.games[game_index].max_playtime)) {
                    char key[5];
                    sprintf(key, "%s", games_array.games[game_index].key);
                    remove_game(&games_array, game_index);
                    sprintf(status, "ETM %c %c %c %c", key[0], key[1], key[2], key[3]);
                } else if (delta_expected_trial_number(&games_array, iPLID, inT) == -1) {
                    if (is_different_than_previous_try(&games_array, iPLID, iC1, iC2, iC3, iC4)) {
                        sprintf(status, "INV");
                    } else {
                        char key[5];
                        sprintf(key, "%s", games_array.games[game_index].key);
                        char try[5];
                        sprintf(try, "%c%c%c%c", iC1, iC2, iC3, iC4);
                        int nB = 0, nW = 0;
                        calculateBlackAndWhite(key, try, &nB, &nW);
                        sprintf(status, "OK %d %d %d", games_array.games[game_index].nT, nB, nW);
                    }
                } else if (delta_expected_trial_number(&games_array, iPLID, inT) == 0) {
                    if (is_repeated_try(&games_array, iPLID, iC1, iC2, iC3, iC4)) {
                        sprintf(status, "DUP");
                    } else {
                        if (inT == 8 && !is_correct_guess(&games_array, iPLID, iC1, iC2, iC3, iC4)) {
                            char key[5];
                            sprintf(key, "%s", games_array.games[game_index].key);
                            remove_game(&games_array, game_index);
                            sprintf(status, "ENT %c %c %c %c", key[0], key[1], key[2], key[3]);
                        } else {
                            char key[5];
                            sprintf(key, "%s", games_array.games[game_index].key);
                            char try[5];
                            sprintf(try, "%c%c%c%c", iC1, iC2, iC3, iC4);
                            int nB = 0, nW = 0;
                            calculateBlackAndWhite(key, try, &nB, &nW);
                            time_t current_time = time(NULL); // Get the current time
                            int time_elapsed = (int)(current_time - games_array.games[game_index].start_time); // Calculate the elapsed time
                            add_try(&games_array.games[game_index], try, time_elapsed);
                            sprintf(status, "OK %d %d %d", games_array.games[game_index].nT, nB, nW);
                        }
                    }
                } else {
                    sprintf(status, "INV");
                }
                sprintf(buffer, "RTR %s\n", status);
                break;
            }

            case QUT: {
                sscanf(args, "%s", PLID);
                int iPLID = get_integer(PLID, 6), game_index = get_game_index(iPLID, &games_array);

                if (iPLID == -1) {
                    sprintf(status, "ERR");
                } else if (game_index != -1) {
                    char key[5];
                    sprintf(key, "%s", games_array.games[game_index].key);
                    remove_game(&games_array, game_index);
                    sprintf(status, "OK %c %c %c %c", key[0], key[1], key[2], key[3]);
                } else {
                    sprintf(status, "NOK");
                }

                sprintf(buffer, "RQT %s\n", status);
                break;
            }
            
            case DBG: {
                char max_playtime[max_size], C1[max_size], C2[max_size], C3[max_size], C4[max_size];
                sscanf(args, "%s %s %s %s %s %s", PLID, max_playtime, C1, C2, C3, C4);
                int iPLID = get_integer(PLID, 6), iTime = get_time(max_playtime), game_index = get_game_index(iPLID, &games_array);
                char iC1 = get_color(C1), iC2 = get_color(C2), iC3 = get_color(C3), iC4 = get_color(C4);

                if (iPLID == -1 || iTime == -1 || iC1 == '\0' || iC2 == '\0' || iC3 == '\0' || iC4 == '\0') {
                    sprintf(status, "%s", "ERR");
                } else if (game_index != -1) {
                    if (has_x_seconds_passed(games_array.games[game_index].start_time, games_array.games[game_index].max_playtime)) {
                        remove_game(&games_array, game_index);
                        add_debug_game(&games_array, iPLID, iTime, iC1, iC2, iC3, iC4);
                        sprintf(status, "%s", "OK");
                    }
                    else
                        sprintf(status, "%s", "NOK");
                } else {
                    add_debug_game(&games_array, iPLID, iTime, iC1, iC2, iC3, iC4);
                    sprintf(status, "%s", "OK");
                }
                sprintf(buffer, "RDB %s\n", status);
                break;
            }
            
            case STR: {
                sscanf(args, "%s", PLID);
                int iPLID = get_integer(PLID, 6), game_index = get_game_index(iPLID, &games_array);
                if (game_index == -1) {
                    sprintf(status, "%s", "NOK");
                } else {
                    char Fname[max_size], Fsize[max_size], Fdata[max_size];
                    int finalized = get_file(Fname, Fsize, Fdata, &games_array, game_index);
                    if (finalized)
                        sprintf(status, "FIN %s %s\n%s", Fname, Fsize, Fdata);
                    else
                        sprintf(status, "ACT %s %s\n%s", Fname, Fsize, Fdata);
                }

                sprintf(buffer, "RST %s\n", status);
                break;
            }

            case SSB: {
                /* Faz `echo` da mensagem recebida para o STDOUT do servidor */
                write(1, "received: ", 10);
                write(1, buffer, strlen(buffer));

                break;
            }
    
        }

        // printf("message sent: %s\n", buffer);
        // print_games(&games_array);
        /* Envia a mensagem recebida (atualmente presente no buffer) para o endereço `addr` de onde foram recebidos dados */
        if (!udp_or_tcp){
            n_udp = sendto(fd_udp, buffer, strlen(buffer), 0, (struct sockaddr *)&addr, addrlen);
            if (n_udp == -1)
                exit(1);
        } else {
            n_tcp = write(newfd_tcp, buffer, strlen(buffer));
            if (n_tcp == -1)
                exit(1);
            close(newfd_tcp);
        }
    }

    freeaddrinfo(res_tcp);
    close(fd_tcp);

    free_game_array(&games_array);
    freeaddrinfo(res_udp);
    close(fd_udp);
}