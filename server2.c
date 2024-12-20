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

#define SNG 0
#define TRY 1
#define QUT 2
#define DBG 3
#define STR 4
#define SSB 5
#define ERR 6

int fd_udp, fd_tcp, newfd_tcp, errcode, max_fd, max_size = 1024;
ssize_t n_udp, n_tcp;
socklen_t addrlen;
struct addrinfo hints_udp, hints_tcp, *res_udp, *res_tcp;
struct sockaddr_in addr;

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

int find_last_game(char *PLID, char *fname)
{
    struct dirent **filelist;
    int n_entries, found;
    char dirname[20];

    sprintf(dirname, "GAMES/%s/", PLID);

    n_entries = scandir(dirname, &filelist, 0, alphasort);

    found = -1;

    if (n_entries <= 0)
        return found;
    else
    {
        while (n_entries--)
        {
            if (filelist[n_entries]->d_name[0] != '.' && !found)
            {
                sprintf(fname, "GAMES/%s/%s", PLID, filelist[n_entries]->d_name);
                found = 1;
            }
            free(filelist[n_entries]);
        }
        free(filelist);
    }

    return found;
}

int create_game() {

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

    char buffer[max_size], PLID[max_size], status[max_size], Fname[50];

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
                int iPLID = get_integer(PLID, 6), iTime = get_time(max_playtime);
                if(find_last_game(PLID, Fname) == -1)
                    create_game();
            }
        }
    }
}