#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "58001"

int fd, errcode;
ssize_t n;
socklen_t addrlen;
struct addrinfo hints, *res;
struct sockaddr_in addr;
char buffer[128];

int main() {
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
    errcode = getaddrinfo(NULL, PORT, &hints, &res);
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

        /* Faz `echo` da mensagem recebida para o STDOUT do servidor */
        write(1, "received: ", 10);
        write(1, buffer, n);

        /* Envia a mensagem recebida (atualmente presente no buffer) para o endereço `addr` de onde foram recebidos dados */
        n = sendto(fd, buffer, n, 0, (struct sockaddr *)&addr, addrlen);
        if (n == -1) {
            exit(1);
        }
    }

    freeaddrinfo(res);
    close(fd);
}