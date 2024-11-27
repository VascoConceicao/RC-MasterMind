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
socklen_t addrlen; // Tamanho do endereço
/*
hints - Estrutura que contém informações sobre o tipo de conexão que será estabelecida.
        Podem-se considerar, literalmente, dicas para o sistema operacional sobre como
        deve ser feita a conexão, de forma a facilitar a aquisição ou preencher dados.

res - Localização onde a função getaddrinfo() armazenará informações sobre o endereço.
*/
struct addrinfo hints, *res;
struct sockaddr_in addr;
char buffer[128]; // buffer para onde serão escritos os dados recebidos do servidor

int main() {

    /* Cria um socket UDP (SOCK_DGRAM) para IPv4 (AF_INET).
    É devolvido um descritor de ficheiro (fd) para onde se deve comunicar. */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        exit(1);
    }

    /* Preenche a estrutura com 0s e depois atribui a informação já conhecida da ligação */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket

    /* Busca informação do host "localhost", na porta especificada,
    guardando a informação nas `hints` e na `res`. Caso o host seja um nome
    e não um endereço IP (como é o caso), efetua um DNS Lookup. */
    errcode = getaddrinfo("localhost", PORT, &hints, &res);
    if (errcode != 0) {
        exit(1);
    }

    /* Envia para o `fd` (socket) a mensagem "Hello!\n" com o tamanho 7.
       Não são passadas flags (0), e é passado o endereço de destino.
       É apenas aqui criada a ligação ao servidor. */
    n = sendto(fd, "Hello!\n", 7, 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) {
        exit(1);
    }

    /* Recebe 128 Bytes do servidor e guarda-os no buffer.
       As variáveis `addr` e `addrlen` não são usadas pois não foram inicializadas. */
    addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, 128, 0, (struct sockaddr *)&addr, &addrlen);
    if (n == -1) {
        exit(1);
    }

    /* Imprime a mensagem "echo" e o conteúdo do buffer (ou seja, o que foi recebido
    do servidor) para o STDOUT (fd = 1) */
    write(1, "echo: ", 6);
    write(1, buffer, n);

    /* Desaloca a memória da estrutura `res` e fecha o socket */
    freeaddrinfo(res);
    close(fd);
}