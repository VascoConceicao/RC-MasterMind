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
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#define SNG 0
#define TRY 1
#define QUT 2
#define DBG 3
#define STR 4
#define SSB 5
#define ERR 6

#define KEY_LENGTH 4

int fd_udp, fd_tcp, newfd_tcp, errcode, max_fd, max_size = 1024;
ssize_t n_udp, n_tcp;
socklen_t addrlen;
struct addrinfo hints_udp, hints_tcp, *res_udp, *res_tcp;
struct sockaddr_in addr;

char valid_colors[] = {'R', 'G', 'B', 'Y', 'O', 'P'};

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

int find_last_game(char *PLID, char *fname) {
    struct dirent **filelist;
    int n_entries, found;
    char dirname[20];

    sprintf(dirname, "GAMES/%s/", PLID);

    n_entries = scandir(dirname, &filelist, 0, alphasort);

    found = 0;

    if (n_entries <= 0)
        return found;
    else {
        while (n_entries--) {
            if (filelist[n_entries]->d_name[0] != '.' && !found) {
                sprintf(fname, "GAMES/%s/%s", PLID, filelist[n_entries]->d_name);
                found = 1;
            }
            free(filelist[n_entries]);
        }
        free(filelist);
    }

    return found;
}

int has_ongoing_game(int PLID) {
    char Fname[max_size];
    sprintf(Fname, "GAME_%06d.txt", PLID);

    DIR *dir = opendir("GAMES");
    struct dirent *entry;

    if (dir == NULL) {
        perror("Unable to open directory");
        return 0; // Directory could not be opened
    }

    while ((entry = readdir(dir)) != NULL) {
        // Check if the entry is a regular file and matches the filename
        if (strcmp(entry->d_name, Fname) == 0) {
            closedir(dir);
            return 1; // File found
        }
    }

    closedir(dir);
    return 0; // File not found
}

int has_x_seconds_passed(time_t start_time, int max_playtime) {
    return (time(NULL) - start_time) > max_playtime;
}

// Function to get the key from the first line of the game file for the given player ID
int get_start_time(int PLID, time_t *start_time) {
    // Construct the filename for the player ID
    char Fname[max_size];
    sprintf(Fname, "GAMES/GAME_%06d.txt", PLID);
    
    FILE *file = fopen(Fname, "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return -1; // Indicating an error
    }

    // Read the first line of the file
    char line[max_size];
    if (fgets(line, sizeof(line), file) == NULL) {
        printf("Error reading the file\n");
        fclose(file);
        return -1; // Indicating an error
    }

    fclose(file);

    // Variables to store the parsed data
    int max_playtime;
    struct tm start_local_time;
    char key[KEY_LENGTH + 1], game_mode;

    // Parse the first line using sscanf
    if (sscanf(line, "%06d %c %4s %d %04d-%02d-%02d %02d:%02d:%02d %ld",
               &PLID, &game_mode, key, &max_playtime,
               &start_local_time.tm_year, &start_local_time.tm_mon, &start_local_time.tm_mday,
               &start_local_time.tm_hour, &start_local_time.tm_min, &start_local_time.tm_sec,
               start_time) != 11) {
        printf("Error parsing the first line\n");
        return -1; // Indicating a parsing error
    }

    // Adjust the year and month as per the `tm` structure expectations
    start_local_time.tm_year -= 1900;  // tm_year is years since 1900
    start_local_time.tm_mon -= 1;      // tm_mon is 0-based (0 = January)

    return 0; // Successfully retrieved the key
}

// Function to get the key from the first line of the game file for the given player ID
int get_max_playtime(int PLID, int *max_playtime) {
    // Construct the filename for the player ID
    char Fname[max_size];
    sprintf(Fname, "GAMES/GAME_%06d.txt", PLID);
    
    FILE *file = fopen(Fname, "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return -1; // Indicating an error
    }

    // Read the first line of the file
    char line[max_size];
    if (fgets(line, sizeof(line), file) == NULL) {
        printf("Error reading the file\n");
        fclose(file);
        return -1; // Indicating an error
    }

    fclose(file);

    // Variables to store the parsed data
    struct tm start_local_time;
    long start_time;
    char key[KEY_LENGTH + 1], game_mode;

    // Parse the first line using sscanf
    if (sscanf(line, "%06d %c %4s %d %04d-%02d-%02d %02d:%02d:%02d %ld",
               &PLID, &game_mode, key, max_playtime,
               &start_local_time.tm_year, &start_local_time.tm_mon, &start_local_time.tm_mday,
               &start_local_time.tm_hour, &start_local_time.tm_min, &start_local_time.tm_sec,
               &start_time) != 11) {
        printf("Error parsing the first line\n");
        return -1; // Indicating a parsing error
    }

    // Adjust the year and month as per the `tm` structure expectations
    start_local_time.tm_year -= 1900;  // tm_year is years since 1900
    start_local_time.tm_mon -= 1;      // tm_mon is 0-based (0 = January)

    return 0; // Successfully retrieved the key
}

void delete_game(int PLID, const char *result, time_t end_time) {
    time_t start_time;
    if (get_start_time(PLID, &start_time) != 0) { // Ensure get_start_time works correctly
        fprintf(stderr, "Error: Could not retrieve start time for PLID %d\n", PLID);
        return;
    }

    // Convert the end time to a struct tm
    struct tm *end_local_time = localtime(&end_time);
    if (!end_local_time) {
        perror("Error converting end time");
        return;
    }

    // Format the timestamp as YYYYMMDD_HHMMSS
    char timestamp[20];
    sprintf(timestamp, "%04d%02d%02d_%02d%02d%02d",
            end_local_time->tm_year + 1900, end_local_time->tm_mon + 1, end_local_time->tm_mday,
            end_local_time->tm_hour, end_local_time->tm_min, end_local_time->tm_sec);

    // Create the directory for the player's ID
    char new_dir[max_size];
    sprintf(new_dir, "GAMES/%06d", PLID);
    if (mkdir(new_dir, 0755) == -1 && errno != EEXIST) {
        perror("Error creating directory");
        return;
    }

    // Construct the new filename with the timestamp and result
    char new_file_name[max_size];
    sprintf(new_file_name, "%s/%s_%s.txt", new_dir, timestamp, result);

    // Original file name
    char original_file[max_size];
    sprintf(original_file, "GAMES/GAME_%06d.txt", PLID);

    // Open the original file for reading
    FILE *file = fopen(original_file, "r");
    if (!file) {
        perror("Error opening original file");
        return;
    }

    // Create the new file for writing
    FILE *new_file = fopen(new_file_name, "w");
    if (!new_file) {
        perror("Error creating new file");
        fclose(file);
        return;
    }

    // Copy the contents of the original file to the new file
    char line[max_size];
    while (fgets(line, sizeof(line), file)) {
        fputs(line, new_file);
    }

    // Calculate the time difference in seconds
    long seconds_difference = difftime(end_time, start_time);

    // Add the last line with the specified format
    char last_line[max_size];
    sprintf(last_line, "%04d-%02d-%02d %02d:%02d:%02d %ld\n",
            end_local_time->tm_year + 1900, end_local_time->tm_mon + 1, end_local_time->tm_mday,
            end_local_time->tm_hour, end_local_time->tm_min, end_local_time->tm_sec,
            seconds_difference);

    fputs(last_line, new_file);

    // Close the files
    fclose(file);
    fclose(new_file);

    // Remove the original file
    if (remove(original_file) == 0) {
        printf("Original file %s removed successfully.\n", original_file);
    } else {
        perror("Error removing the original file");
    }

    printf("Game data moved to: %s\n", new_file_name);
}

void check_time(int PLID) {
    // Does not have an ongoing game, do not need to check time
    if (!has_ongoing_game(PLID))
        return;

    char Fname[max_size];
    sprintf(Fname, "GAMES/GAME_%06d.txt", PLID);

    FILE *file = fopen(Fname, "r");
    if (!file) {
        perror("Error opening file");
        return;
    }

    // Variables to hold the parsed values
    int gamePLID, max_playtime;
    char key[KEY_LENGTH + 1], game_mode;
    struct tm start_local_time;
    time_t start_time;

    // Reading the first line from the file
    char buffer[max_size];
    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        int result = sscanf(buffer, "%6d %c %s %d %4d-%2d-%2d %2d:%2d:%2d %ld\n",
                            &gamePLID, &game_mode, key, &max_playtime,
                            &start_local_time.tm_year, &start_local_time.tm_mon, &start_local_time.tm_mday,
                            &start_local_time.tm_hour, &start_local_time.tm_min, &start_local_time.tm_sec,
                            &start_time);

        if (result == 10) {
            if (has_x_seconds_passed(start_time, max_playtime)) {
                // Call delete_game with the "L" result for loss
                delete_game(PLID, "L", start_time + max_playtime);
            } else {
                printf("Game is still ongoing. No action taken.\n");
            }
        } else {
            printf("Error: Failed to parse the line correctly.\n");
        }
    } else {
        printf("Error: Could not read from the file.\n");
    }

    fclose(file);
}

void create_new_play_game(int PLID, int max_playtime) {
    // Define a buffer for the filename
    char key[KEY_LENGTH + 1], Fname[max_size], Fdata[max_size];

    // Seed the random number generator
    srand(time(NULL));

    // Generate a random KEY_LENGTH-character string
    for (int i = 0; i < KEY_LENGTH; i++) {
        key[i] = valid_colors[rand() % sizeof(valid_colors)];
    }
    key[KEY_LENGTH + 1] = '\0'; // Null-terminate the string
    
    // Create the filename using PLID
    sprintf(Fname, "GAMES/GAME_%06d.txt", PLID);

    // Get the current time
    time_t start_time;
    time(&start_time);

    // Convert the current time to a struct tm
    struct tm *start_local_time = localtime(&start_time);

    sprintf(Fdata, "%06d P %s %d %04d-%02d-%02d %02d:%02d:%02d %ld\n", PLID, key, max_playtime, start_local_time->tm_year + 1900, start_local_time->tm_mon + 1, start_local_time->tm_mday, start_local_time->tm_hour, start_local_time->tm_min, start_local_time->tm_sec, start_time);

    // Open the file for writing
    FILE *file = fopen(Fname, "w");
    if (file == NULL) {
        perror("Error creating file");
        return;
    }

    // Write Fdata to the file
    fprintf(file, "%s", Fdata);

    printf("File %s created\n", Fname);

    // Close the file
    fclose(file);
}

void create_new_debug_game(int PLID, int max_playtime, char key[KEY_LENGTH + 1]) {
    // Define a buffer for the filename
    char Fname[max_size], Fdata[max_size];

    // Seed the random number generator
    srand(time(NULL));

    // Create the filename using PLID
    sprintf(Fname, "GAMES/GAME_%06d.txt", PLID);

    // Get the current time
    time_t start_time;
    time(&start_time);

    // Convert the current time to a struct tm
    struct tm *start_local_time = localtime(&start_time);

    sprintf(Fdata, "%06d D %s %d %04d-%02d-%02d %02d:%02d:%02d %ld\n", PLID, key, max_playtime, start_local_time->tm_year + 1900, start_local_time->tm_mon + 1, start_local_time->tm_mday, start_local_time->tm_hour, start_local_time->tm_min, start_local_time->tm_sec, start_time);

    // Open the file for writing
    FILE *file = fopen(Fname, "w");
    if (file == NULL) {
        perror("Error creating file");
        return;
    }

    // Write Fdata to the file
    fprintf(file, "%s", Fdata);

    printf("File %s created\n", Fname);

    // Close the file
    fclose(file);
}

int expected_trial_number(int PLID) {
    char Fname[max_size];
    sprintf(Fname, "GAMES/GAME_%06d.txt", PLID);

    FILE *file;
    char ch;
    int lineCount = 0;

    // Open the file for reading
    file = fopen(Fname, "r");

    // Check if file was opened successfully
    if (file == NULL) {
        printf("Error opening the file.\n");
        return 1;
    }

    // Read through the file character by character
    while ((ch = fgetc(file)) != EOF) {
        // Increment line count when a newline character is encountered
        if (ch == '\n') {
            lineCount++;
        }
    }

    // If the file isn't empty and doesn't end with a newline, count the last line
    if (ch != '\n' && ftell(file) > 0) {
        lineCount++;
    }

    // Close the file
    fclose(file);

    return lineCount - 1;
}

// Function to get the last try from the game file for the given player ID
int get_last_try(int PLID, char try[KEY_LENGTH + 1]) {
    // Construct the filename for the player ID
    char filename[max_size];
    sprintf(filename, "GAMES/GAME_%06d.txt", PLID);
    
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return -1; // Indicating an error
    }

    // Seek to the end of the file and find the last line
    char line[max_size];
    char last_line[max_size];
    last_line[0] = '\0';

    while (fgets(line, sizeof(line), file)) {
        // Store the current line as the last line
        strcpy(last_line, line);
    }

    fclose(file);

    // Parse the last line using sscanf
    if (sscanf(last_line, "T: %4s", try) != 1) {
        printf("Error parsing the last line\n");
        return -1; // Indicating a parsing error
    }

    return 0; // Successfully retrieved the last try
}

// Function to get the key from the first line of the game file for the given player ID
int get_key(int PLID, char key[KEY_LENGTH + 1]) {
    // Construct the filename for the player ID
    char Fname[max_size];
    sprintf(Fname, "GAMES/GAME_%06d.txt", PLID);
    
    FILE *file = fopen(Fname, "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return -1; // Indicating an error
    }

    // Read the first line of the file
    char line[max_size];
    if (fgets(line, sizeof(line), file) == NULL) {
        printf("Error reading the file\n");
        fclose(file);
        return -1; // Indicating an error
    }

    fclose(file);

    // Variables to store the parsed data
    int max_playtime;
    struct tm start_local_time;
    long start_time;
    char game_mode;

    // Parse the first line using sscanf
    if (sscanf(line, "%06d %c %4s %d %04d-%02d-%02d %02d:%02d:%02d %ld",
               &PLID, &game_mode, key, &max_playtime,
               &start_local_time.tm_year, &start_local_time.tm_mon, &start_local_time.tm_mday,
               &start_local_time.tm_hour, &start_local_time.tm_min, &start_local_time.tm_sec,
               &start_time) != 11) {
        printf("Error parsing the first line\n");
        return -1; // Indicating a parsing error
    }

    // Adjust the year and month as per the `tm` structure expectations
    start_local_time.tm_year -= 1900;  // tm_year is years since 1900
    start_local_time.tm_mon -= 1;      // tm_mon is 0-based (0 = January)

    return 0; // Successfully retrieved the key
}

// Function to check if the current try is different than the previous one
int is_different_than_previous_try(int PLID, char try[KEY_LENGTH + 1]) {
    // First try
    if (expected_trial_number(PLID) == 1)
        return 1;
    
    char last_try[KEY_LENGTH + 1];
    get_last_try(PLID, last_try);

    // Compare the current 'try' with the last 'try'
    if (strcmp(try, last_try) == 0) {
        return 0; // No difference, same try
    } else {
        return 1; // Different try
    }
}

void calculateBlackAndWhite(int PLID, char try[KEY_LENGTH + 1], int *nB, int *nW) {
    char key[KEY_LENGTH + 1];

    // Get the secret key from the file
    if (get_key(PLID, key) != 0) {
        printf("Error retrieving the key\n");
        return; // Exit if there is an error in retrieving the key
    }

    int black = 0, white = 0;

    // Arrays to track counts of unmatched characters
    int secretCount[256] = {0};
    int guessCount[256] = {0};

    // First pass: count black matches
    for (int i = 0; i < KEY_LENGTH; i++) {
        if (key[i] == try[i]) {
            black++;
        } else {
            // Track unmatched characters
            secretCount[(unsigned char)key[i]]++;
            guessCount[(unsigned char)try[i]]++;
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


// Function to check if a try has been repeated in the game file
int is_repeated_try(int PLID, char try[KEY_LENGTH + 1]) {
    // Construct the filename for the player ID
    char filename[max_size];
    sprintf(filename, "GAMES/GAME_%06d.txt", PLID);
    
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return -1; // Error opening the file
    }

    // Skip the first line (header)
    char line[max_size];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0; // No lines after the first, so no repeated try
    }

    // Check each subsequent line for the trial sequence (CCCC)
    while (fgets(line, sizeof(line), file)) {
        char trial[KEY_LENGTH + 1]; // To store the CCCC sequence
        int B, W, s;    // These are the other components in the line (not used here)
        
        // Parse the line to get the trial sequence
        if (sscanf(line, "T: %4s %d %d %d", trial, &B, &W, &s) == 4) {
            // Compare the trial sequence with the given try
            if (strcmp(try, trial) == 0) {
                fclose(file);
                return 1; // Found a repeated try
            }
        }
    }

    fclose(file);
    return 0; // No repeated try found
}

int is_correct_guess(int PLID, char try[KEY_LENGTH + 1]) {
    char key[KEY_LENGTH + 1];
    get_key(PLID, key);
    for (int i = 0; i < KEY_LENGTH; i++)
        if (try[i] != key[i])
            return 0;
    return 1;
}

// Function to add a try to the game file
void add_try(int PLID, char try[KEY_LENGTH + 1]) {
    // File name based on the player ID
    char filename[max_size];
    sprintf(filename, "GAMES/GAME_%06d.txt", PLID);
    
    FILE *file = fopen(filename, "a");  // Open the file in append mode
    if (file == NULL) {
        printf("Error opening file\n");
        return; // Exit if there is an error opening the file
    }

    // Calculate the number of black and white matches for the given try
    int nB = 0, nW = 0;
    calculateBlackAndWhite(PLID, try, &nB, &nW);

    // Get the start time from the first line of the file
    char line[max_size];
    long start_time = 0;
    FILE *temp_file = fopen(filename, "r");
    if (temp_file == NULL) {
        printf("Error opening file for reading\n");
        fclose(file);
        return;
    }

    // Read the first line to extract the start time
    if (fgets(line, sizeof(line), temp_file) != NULL) {
        int max_playtime;
        char key[KEY_LENGTH + 1], game_mode;
        struct tm start_local_time;
        if (sscanf(line, "%06d %c %4s %d %04d-%02d-%02d %02d:%02d:%02d %ld",
                   &PLID, &game_mode, key, &max_playtime,
                   &start_local_time.tm_year, &start_local_time.tm_mon, &start_local_time.tm_mday,
                   &start_local_time.tm_hour, &start_local_time.tm_min, &start_local_time.tm_sec,
                   &start_time) != 11) {
            printf("Error parsing the first line\n");
            fclose(temp_file);
            fclose(file);
            return;
        }
    }
    fclose(temp_file);

    // Calculate the elapsed time (s) since the start of the game
    long elapsed_time = time(NULL) - start_time;

    // Write the new trial to the file in the specified format
    fprintf(file, "T: %4s %d %d %ld\n", try, nB, nW, elapsed_time);

    printf("Devia ter escrito\n");

    if (nB == 4)
        delete_game(PLID, "W", time(NULL));

    fclose(file);
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
    if (fd_udp == -1)
        exit(1);

    memset(&hints_udp, 0, sizeof hints_udp);
    hints_udp.ai_family = AF_INET;
    hints_udp.ai_socktype = SOCK_DGRAM;
    /* É passada uma flag para indicar que o socket é passivo.
    Esta flag é usada mais tarde pela função `bind()` e indica que
    o socket aceita conceções. */
    hints_udp.ai_flags = AI_PASSIVE;

    /* Ao passar o endereço `NULL`, indicamos que somos nós o Host. */
    errcode = getaddrinfo(NULL, GSport, &hints_udp, &res_udp);
    if (errcode != 0)
        exit(1);

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

    char buffer[max_size], PLID[max_size], status[max_size], Fname[max_size];

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
        } else {
            command = ERR;
        }

        switch (command) {
            case SNG: {
                char max_playtime[max_size];
                sscanf(args, "%s %s", PLID, max_playtime);
                int iPLID = get_integer(PLID, 6), imax_playime = get_time(max_playtime);
                
                if (iPLID == -1 || imax_playime == -1) {
                    sprintf(buffer, "RSG ERR\n");
                    break;
                }

                check_time(iPLID);
                if (has_ongoing_game(iPLID)) {
                    sprintf(status, "NOK");
                } else {
                    create_new_play_game(iPLID, imax_playime);
                    sprintf(status, "OK");
                }

                sprintf(buffer, "RSG %s\n", status);
                break;
            }

            case TRY: {
                char try[KEY_LENGTH + 1], C1[max_size], C2[max_size], C3[max_size], C4[max_size], nT[max_size];
                sscanf(args, "%s %s %s %s %s %s", PLID, C1, C2, C3, C4, nT);
                int iPLID = get_integer(PLID, 6), inT = get_integer(nT, 1);
                char iC1 = get_color(C1), iC2 = get_color(C2), iC3 = get_color(C3), iC4 = get_color(C4);
                sprintf(try, "%c%c%c%c", iC1, iC2, iC3, iC4);

                if (iPLID == -1 || inT == -1 || iC1 == '\0' || iC2 == '\0' || iC3 == '\0' || iC4 == '\0') {
                    sprintf(buffer, "RTR ERR\n");
                    break;
                }

                int had_ongoing_game = has_ongoing_game(iPLID);
                char key[KEY_LENGTH + 1];
                if (had_ongoing_game)
                    get_key(iPLID, key);
                check_time(iPLID);

                if (!has_ongoing_game(iPLID)) {
                    if (had_ongoing_game)
                        sprintf(status, "ETM %c %c %c %c", key[0], key[1], key[2], key[3]);
                    else
                        sprintf(status, "NOK");
                } else if (inT - expected_trial_number(iPLID) == -1) {
                    if (is_different_than_previous_try(iPLID, try)) {
                        sprintf(status, "INV");
                    } else {
                        int nB = 0, nW = 0;
                        calculateBlackAndWhite(iPLID, try, &nB, &nW);
                        sprintf(status, "OK %d %d %d", inT, nB, nW);
                    }
                } else if (inT - expected_trial_number(iPLID) == 0) {
                    if (is_repeated_try(iPLID, try)) {
                        sprintf(status, "DUP");
                    } else {
                        if (inT == 8 && !is_correct_guess(iPLID, try)) {
                            sprintf(status, "ENT %c %c %c %c", key[0], key[1], key[2], key[3]);
                        }
                        else {
                            int nB = 0, nW = 0;
                            calculateBlackAndWhite(iPLID, try, &nB, &nW);
                            sprintf(status, "OK %d %d %d", inT, nB, nW);
                        }
                        add_try(iPLID, try);
                    }
                } else {
                    sprintf(status, "INV");
                }
                
                sprintf(buffer, "RTR %s\n", status);
                break;

            }

            case QUT: {
                sscanf(args, "%s", PLID);
                int iPLID = get_integer(PLID, 6);

                if (iPLID == -1) {
                    sprintf(buffer, "RQT ERR\n");
                    break;
                }

                check_time(iPLID);
                if (has_ongoing_game(iPLID)) {
                    char key[KEY_LENGTH + 1];
                    get_key(iPLID, key);
                    delete_game(iPLID, "L", time(NULL));
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
                int iPLID = get_integer(PLID, 6), iTime = get_time(max_playtime);
                char iC1 = get_color(C1), iC2 = get_color(C2), iC3 = get_color(C3), iC4 = get_color(C4);

                if (iPLID == -1 || iTime == -1 || iC1 == '\0' || iC2 == '\0' || iC3 == '\0' || iC4 == '\0') {
                    sprintf(buffer, "RDB ERR\n");
                    break;
                }

                check_time(iPLID);
                if (has_ongoing_game(iPLID)) {
                    sprintf(status, "NOK");
                } else {
                    char key[KEY_LENGTH + 1];
                    sprintf(key, "%c%c%c%c", iC1, iC2, iC3, iC4);
                    create_new_debug_game(iPLID, iTime, key);
                    sprintf(status, "OK");
                }

                sprintf(buffer, "RDB %s\n", status);
                break;
            }

            case STR: {
                if (has_ongoing_game(iPLID)) {
                    check_time(iPLID);
                    send_summary(iPLID);
                    sprintf(status, "ACT");
                }

                sprintf(buffer, "RST %s\n", status);
                break;
            }
        }

        if (!udp_or_tcp){
            printf("Sent: %s\n", buffer);
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
}