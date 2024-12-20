#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int fd_udp, fd_tcp, errcode;
ssize_t n_udp, n_tcp;
socklen_t addrlen; // Tamanho do endereço
/*
hints - Estrutura que contém informações sobre o tipo de conexão que será estabelecida.
        Podem-se considerar, literalmente, dicas para o sistema operacional sobre como
        deve ser feita a conexão, de forma a facilitar a aquisição ou preencher dados.

res - Localização onde a função getaddrinfo() armazenará informações sobre o endereço.
*/
struct addrinfo hints_udp, hints_tcp, *res_udp, *res_tcp;
struct sockaddr_in addr;

int n_spaces(char *args) {
    int n = 0;
    for (int i = 1; args[i] != '\0'; i++) {
        if (args[i] == ' ' && args[i - 1] != ' ')
            n += 1;
    }
    return args[strlen(args) - 1] == ' ' ? n - 1 : n;
}

int main(int argc, char *argv[]) {
    char *GSIP = "localhost";
    char* GSport = "58080";

    int opt;
    while ((opt = getopt(argc, argv, "n:p:")) != -1) {
        switch (opt) {
            case 'n':
                GSIP = optarg;
                break;
            case 'p':
                GSport = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [-n GSIP] [-p GSport]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    printf("GSIP: %s\n", GSIP);
    printf("GSport: %s\n", GSport);

    /* Cria um socket UDP (SOCK_DGRAM) para IPv4 (AF_INET).
    É devolvido um descritor de ficheiro (fd_udp) para onde se deve comunicar. */
    fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == -1)
        exit(1);

    /* Preenche a estrutura com 0s e depois atribui a informação já conhecida da ligação */
    memset(&hints_udp, 0, sizeof hints_udp);
    hints_udp.ai_family = AF_INET;      // IPv4
    hints_udp.ai_socktype = SOCK_DGRAM; // UDP socket

    /* Busca informação do host "localhost", na porta especificada,
    guardando a informação nas `hints` e na `res`. Caso o host seja um nome
    e não um endereço IP (como é o caso), efetua um DNS Lookup. */
    errcode = getaddrinfo(GSIP, GSport, &hints_udp, &res_udp);
    if (errcode != 0)
        exit(1);

    int max_size = 1024, nT = 1;
    char buffer[max_size], PLID[max_size], command[max_size], temp_PLID[max_size], args[max_size], next[max_size];
    PLID[0] = '\0';

    int ok = 1;
    while (ok) {
        memset(buffer, 0, sizeof(buffer));
        memset(command, 0, sizeof(command));
        memset(args, 0, sizeof(args));
        memset(next, 0, sizeof(next));
        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
            exit(1);

        sscanf(buffer, "%s %[^\n]", command, args);
        int mode = 0;
        if (!strcmp(command, "start") && n_spaces(args) == 1) {
            char max_playtime[max_size];
            sscanf(args, "%s %s %[^\n]", temp_PLID, max_playtime, next);
            sprintf(buffer, "%s %s %s\n", "SNG", temp_PLID, max_playtime);
        } else if (!strcmp(command, "try") && n_spaces(args) == 3) {
            char C1[max_size], C2[max_size], C3[max_size], C4[max_size];
            sscanf(args, "%s %s %s %s %[^\n]", C1, C2, C3, C4, next);
            sprintf(buffer, "%s %s %s %s %s %s %d\n", "TRY", PLID, C1, C2, C3, C4, nT);
        } else if (!strcmp(command, "show_trials") || !strcmp(command, "st")) {
            mode = 1;
            sprintf(next, "%s", args);
            sprintf(buffer, "%s %s\n", "STR", PLID);
        } else if (!strcmp(command, "scoreboard") || !strcmp(command, "sb")) {
            mode = 1;
            sprintf(next, "%s", args);
            sprintf(buffer, "%s\n", "SSB");
        } else if (!strcmp(command, "quit")) {
            sprintf(next, "%s", args);
            sprintf(buffer, "%s %s\n", "QUT", PLID);
        } else if (!strcmp(command, "exit")) {
            ok = 0;
            sprintf(next, "%s", args);
            sprintf(buffer, "%s %s\n", "QUT", PLID);
        } else if (!strcmp(command, "debug") && n_spaces(args) == 5) {
            char max_playtime[max_size], C1[max_size], C2[max_size], C3[max_size], C4[max_size];
            sscanf(args, "%s %s %s %s %s %s %[^\n]", temp_PLID, max_playtime, C1, C2, C3, C4, next);
            sprintf(buffer, "%s %s %s %s %s %s %s\n", "DBG", temp_PLID, max_playtime, C1, C2, C3, C4);
        } else {
            sprintf(buffer, "%s\n", "ERR");
        }
        if (strcmp(next, ""))
            sprintf(buffer, "%s\n", "ERR");
        
        switch (mode) {
            // Modo UDP
            case 0:
                /* Envia para o `fd_udp` (socket) a mensagem "Hello!\n" com o tamanho 7.
                Não são passadas flags (0), e é passado o endereço de destino.
                É apenas aqui criada a ligação ao servidor. */
                n_udp = sendto(fd_udp, buffer, strlen(buffer), 0, res_udp->ai_addr, res_udp->ai_addrlen);
                if (n_udp == -1)
                    exit(1);

                /* Recebe 128 Bytes do servidor e guarda-os no buffer.
                As variáveis `addr` e `addrlen` não são usadas pois não foram inicializadas. */
                addrlen = sizeof(addr);
                n_udp = recvfrom(fd_udp, buffer, max_size, 0, (struct sockaddr *)&addr, &addrlen);
                if (n_udp == -1)
                    exit(1);

                /* Imprime a mensagem "echo" e o conteúdo do buffer (ou seja, o que foi recebido
                do servidor) para o STDOUT (fd_udp = 1) */
                write(1, "response: ", 10);
                write(1, buffer, n_udp);

                if (!strncmp(buffer, "RSG OK", 6) || !strncmp(buffer, "RDB OK", 6)) {
                    strcpy(PLID, temp_PLID);
                    nT = 1;
                } else if (!strncmp(buffer, "RTR OK", 6))
                    nT = (buffer[7] - '0') + 1;
                break;

            // Modo TCP
            case 1:
                /* Cria um socket TCP (SOCK_STREAM) para IPv4 (AF_INET).
                É devolvido um descritor de ficheiro (fd) para onde se deve comunicar. */
                fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
                if (fd_tcp == -1)
                    exit(1);

                /* Preenche a estrutura com 0s e depois atribui a informação já conhecida da ligação */
                memset(&hints_tcp, 0, sizeof hints_tcp);
                hints_tcp.ai_family = AF_INET;
                hints_tcp.ai_socktype = SOCK_STREAM; // TCP socket

                errcode = getaddrinfo(GSIP, GSport, &hints_tcp, &res_tcp);
                if (errcode != 0)
                    exit(1);

                /* Em TCP é necessário estabelecer uma ligação com o servidor primeiro (Handshake).
                Então primeiro cria a conexão para o endereço obtido através de `getaddrinfo()`. */
                n_tcp = connect(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen);
                if (n_tcp == -1)
                    exit(1);

                size_t total_bytes_written = 0;
                while (total_bytes_written < strlen(buffer)) {
                    n_tcp=write(fd_tcp, buffer + total_bytes_written, strlen(buffer) - total_bytes_written);
                    if (n_tcp == -1)
                        exit(1);
                    total_bytes_written += n_tcp;
                }

                char *response = NULL;
                size_t total_size = 0;
                
                while ((n_tcp = read(fd_tcp, buffer, 4)) > 0) {
                    char *temp = realloc(response, total_size + n_tcp + 1);
                    if (!temp) {
                        free(response);
                        exit(1);
                    }
                    response = temp;
                    memcpy(response + total_size, buffer, n_tcp);
                    total_size += n_tcp;
                    response[total_size] = '\0';
                }

                if (n_tcp == -1) {
                    free(response);
                    exit(1);
                }
                
                write(1, "response: ", 10);
                write(1, response, strlen(response));

                char status[max_size], Fname[max_size], Fsize[max_size];
                const char *remaining = response;
                sscanf(remaining, "%*s %9s %255s %255s", status, Fname, Fsize);
                
                if (!strcmp(status, "ACT") || !strcmp(status, "FIN") || !strcmp(status, "OK")) {
                    remaining = strstr(remaining, Fsize);
                    if (remaining == NULL) exit(1);
                    remaining += strlen(Fsize);
                    while (*remaining == ' ') remaining++;
                    const char *Fdata = remaining;
                    // printf("status: %s\nFname: %s\nFsize: %s\nFdata: %s\n", status, Fname, Fsize, Fdata);
                    FILE *fd = fopen(Fname, "w");
                    if (fd == NULL)
                        exit(1);
                    fprintf(fd, "%s", Fdata);
                    fclose(fd);
                }

                free(response);
                freeaddrinfo(res_tcp);
                close(fd_tcp);
                break;
        }
    }

    freeaddrinfo(res_udp);
    close(fd_udp);
}