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

int main(int argc, char *argv[]) {

    int max_size = 512;
    char buffer[max_size]; // buffer para onde serão escritos os dados recebidos do servidor

    char *GSIP = "localhost";
    char* GSport = "58080";

    int opt;
    char *endptr;

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

    int ok = 1, ok_flag = 0;
    while (ok) {
        buffer[0] = '\0';
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("Error reading input.\n");
            exit(1);
        }

        char command[max_size], args[max_size];
        sscanf(buffer, "%s %[^\n]", command, args);
        int mode = 0;
        if (!strcmp(command, "start")) {
            strcpy(command, "SNG");
        } else if (!strcmp(command, "try")) {
            strcpy(command, "TRY");
        } else if (!strcmp(command, "show_trials") || !strcmp(command, "st")) {
            mode = 1;
            strcpy(command, "STR");
        } else if (!strcmp(command, "scoreboard") || !strcmp(command, "sb")) {
            mode = 1;
            strcpy(command, "SSB");
        } else if (!strcmp(command, "quit")) {
            strcpy(command, "QUT");
        } else if (!strcmp(command, "exit")) {
            strcpy(command, "QUT");
            ok_flag = 1;
        } else if (!strcmp(command, "debug")) {
            strcpy(command, "DBG");
        } else {
            strcpy(command, "ERR");
        }
        sprintf(buffer, "%s %s\n", command, args);
        command[0] = '\0';
        args[0] = '\0';
        // 1 erro: "st 106481 qualquer coisa" ou "show trials 106481 qq coisa" da erro no read (-1, entra na linha 181) e acaba o programa (erro do server?)
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
                
                if (ok_flag && !strncmp(buffer, "RQT OK", 6))
                    ok = 0;
                else if (ok_flag)
                    ok_flag = 0;
                break;

            // Modo TCP
            case 1:

                /* Cria um socket TCP (SOCK_STREAM) para IPv4 (AF_INET).
                É devolvido um descritor de ficheiro (fd) para onde se deve comunicar. */
                fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
                if (fd_tcp == -1) {
                    exit(1);
                }

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

                ssize_t total_bytes_written = 0;
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

                char status[10] = {0};
                char Fname[256] = {0};
                char Fsize[256] = {0};
                const char *remaining = response;
                sscanf(remaining, "%*s %9s %255s %255s", status, Fname, Fsize);
                remaining = strstr(remaining, Fsize);
                if (remaining == NULL) exit(1);
                remaining += strlen(Fsize);
                while (*remaining == ' ') remaining++;
                const char *Fdata = remaining;
                // printf("status: %s\nFname: %s\nFsize: %s\nFdata: %s\n", status, Fname, Fsize, Fdata);
                
                if (!strcmp(status, "ACT") || !strcmp(status, "FIN") || !strcmp(status, "OK")) {
                    FILE *fd = fopen(Fname, "w");
                    if (fd == NULL) {
                        return 1; // Error opening the file
                    }
            
                    // Write the remaining data to the file
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