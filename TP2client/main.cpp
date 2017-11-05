#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

using namespace std;

#define STDIN 0  // file descriptor for standard input
#define BUFSZ 131068    //Numero de bytes necessarios para receber os ids
                        //dos clientes em CLIST caso todos estejam conectados.
#define OK 1
#define ERRO 2
#define OI 3
#define FLW 4
#define MSG 5
#define CREQ 6
#define CLIST 7
#define SERV 65535

uint16_t selfId;        //Identificador do cliente.
int sock;               //Socket conectado ao servidor.

int get_header(uint16_t *type, uint16_t *idOrig, uint16_t *idDest, uint16_t *seq){
    int nBytes;
    uint8_t buf[BUFSZ];
    if ((nBytes = recv(sock, buf, 8, MSG_WAITALL)) < 8){
        // got error or connection closed by server
        if (nBytes < 0) {
            perror("recv");
        } else{
            printf("Servidor não está respondendo corretamente.\n");
        }
        printf("Encerrando cliente.\n");
        close(sock);
        return 1;
    }
    *type = ntohs(*(uint16_t*) buf);
    *idOrig = ntohs(*(uint16_t*) (buf + 2));
    *idDest = ntohs(*(uint16_t*) (buf + 4));
    *seq = ntohs(*(uint16_t*) (buf + 6));
    return 0;
}

int send_msg(uint16_t type, uint16_t idDest, uint16_t seq, char *txt, int length){
    uint8_t msg[BUFSZ];
    int nBytes;
    ((uint16_t*)msg)[0] = htons(type);
    ((uint16_t*)msg)[1] = htons(selfId);
    ((uint16_t*)msg)[2] = htons(idDest);
    ((uint16_t*)msg)[3] = htons(seq);
    if ((nBytes = send(sock, msg, 8, MSG_NOSIGNAL)) < 8){
        if (nBytes < 0) {
            perror("send");
        } else {
            printf("Servidor não está respondendo corretamente.\n");
        }
        printf("Encerrando cliente.\n");
        close(sock);
        return 1;
    }

    if (type == MSG) {
        ((uint16_t*)msg)[0] = htons(length);
        if ((nBytes = send(sock, msg, 2, MSG_NOSIGNAL)) < 2){
            if (nBytes < 0) {
                perror("send");
            } else {
                printf("Servidor não está respondendo corretamente.\n");
            }
            printf("Encerrando cliente.\n");
            close(sock);
            return 1;
        }

        if ((nBytes = send(sock, txt, length, MSG_NOSIGNAL)) < length){
            if (nBytes < 0) {
                perror("send");
            } else {
                printf("Servidor não está respondendo corretamente.\n");
            }
            printf("Encerrando cliente.\n");
            close(sock);
            return 1;
        }
    }

    return 0;
}

int inicia_cliente(){
    uint16_t type;
    uint16_t idOrig;
    uint16_t idDest;
    uint16_t seq;

    selfId = 0;     //O cliente ainda não tem id proprio.
    if (send_msg(OI, SERV, 0, "", 0) != 0)
        return 1;

    if (get_header(&type, &idOrig, &idDest, &seq) != 0)
        return 1;

    if (type != OK){
        printf("Servidor não está respondendo corretamente.\n");
        printf("Encerrando cliente.\n");
        close(sock);
        return 1;
    }

    selfId = idDest;
    printf("Seu número identificador é: %d\n", selfId);
    return 0;
}

int get_txt(int mustRead, uint8_t *buf){
    int nBytes;
    if ((nBytes = recv(sock, buf, mustRead, MSG_WAITALL)) < mustRead){
        // got error or connection closed by server
        if (nBytes < 0) {
            perror("recv");
        } else {
            // connection closed
            printf("Servidor não está respondendo corretamente.\n");
        }
        printf("Encerrando cliente.\n");
        close(sock);
        return 1;
    }
    return 0;
}

int main(int argc, char * argv[])
{
    int nClients;   //Numero de clientes retornado pelo CLIST.
    uint16_t type;  //Tipo da mensagem recebida.
    uint16_t idOrig;    //ID do remetente da mensagem.
    uint16_t idDest;    //ID do destinatario da mensagem.
    uint16_t seq;       //Numero de sequencia da mensagem.
    uint8_t buf[BUFSZ]; //Buffer para fazer leitura do socket.
    char txt[401];      //String para armazenar o texto digitado pelo usuario.
    char option;        //Opção escolhida pelo usuario.
    uint16_t counter = 1;   //Contador para o numero de sequencia da mensagem.
    uint16_t tamanhoMsg;
    int portno = atoi(argv[2]);

    int seqFLW;             //Armazena o numero de sequencia da mensagem de FLW
                            //para poder conferir quando o servidor responder um OK.

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return 0;
    }

    struct in_addr addr;
    //Seta ip do servidor.
    if (inet_pton(AF_INET, argv[1], &addr) < 0) {
        perror("pton");
        printf("Encerrando cliente.\n");
        return 0;
    }

    struct sockaddr_in dst = {.sin_family = AF_INET,
        .sin_port = htons(portno),
        .sin_addr = addr
    };

    if (connect(sock, (struct sockaddr * ) & dst, sizeof(dst)) == -1){
        perror("connect");
        printf("Encerrando cliente.\n");
        return 0;
    }

    fd_set readfds, master;         //Set de streams que vamos vigiar para leitura.
    FD_ZERO(&readfds);
    FD_ZERO(&master);
    FD_SET(STDIN, &master);
    FD_SET(sock, &master);        //Adiciona o socket ao set de leituras a ser observado.

    if (inicia_cliente() != 0)
        return 0;

    while (1){
        readfds = master; // copy it
        if (select(sock + 1, &readfds, NULL, NULL, NULL) == -1){ //Observa o set readfds.
            close(sock);
            perror("select");
            printf("Encerrando cliente.\n");
            return 0;
        }

        if (FD_ISSET(STDIN, &readfds)){
            scanf("%c", &option);
            switch (option){
                case 'M' :      //O usuario quer enviar uma mensagem.
                    scanf("%d", &idDest);
                    fgets (txt, 401, stdin);
                    if (send_msg(MSG, idDest, counter, txt, strlen(txt) + 1) != 0)
                        return 0;
                    counter++;
                    break;
                case 'L' :      //O usuario quer ver uma lista dos clientes conectados.
                    if(send_msg(CREQ, SERV, counter, "", 0) != 0)
                        return 0;
                    counter++;
                    break;
                case 'S' :      //O usuario quer se desconectar.
                    if (send_msg(FLW, SERV, counter, "", 0) != 0)
                        return 0;
                    seqFLW = counter;
                    counter++;
                    break;
                default :
                    printf("Opção não reconhecida.\n Tecle M para enviar uma mensagem.\n Tecle L para listar os destinatários disponíveis.\n Ou tecle S para sair.\n");
            }
        }
        if (FD_ISSET(sock, &readfds)){
            if (get_header(&type, &idOrig, &idDest, &seq) != 0)
                return 0;

            switch (type){
                case OK :
                    if (seqFLW != -1)
                        if (seq == (uint16_t) seqFLW){
                            printf("Encerrando cliente.\n");
                            close(sock);
                            return 0;
                        }
                    break;
                case ERRO :
                    break;
                case MSG :
                    if (get_txt(2, buf) != 0)
                        return 0;

                    tamanhoMsg = ntohs(*(uint16_t*) buf);

                    if (get_txt(tamanhoMsg, buf) != 0)
                        return 0;

                    if (selfId != idDest && idDest != 0){
                        if(send_msg(ERRO, idOrig, seq, "", 0) != 0)
                            return 0;
                    } else {
                        if (send_msg(OK, idOrig, seq, "", 0) != 0)
                            return 0;
                        printf("Mensagem de %d: %s\n", idOrig, (char *) buf);
                    }
                    break;
                case CLIST:
                    if (get_txt(2, buf) != 0)
                        return 0;

                    nClients = ntohs(*(uint16_t*) buf);
                    if (get_txt(2 * nClients, buf) != 0)
                        return 0;

                    if (send_msg(OK, idOrig, seq, "", 0) != 0)
                        return 0;
                    printf("Clientes disponíveis:\n");

                    for (int i = 0; i < nClients; i++)
                        printf("%d\n", ntohs(*(uint16_t*) (buf + 2 * i)));

                    break;
                default :
                    if (send_msg(ERRO, idOrig, seq, "", 0) != 0)
                        return 0;
            }
        }
    }
}
