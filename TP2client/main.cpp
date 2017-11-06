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

#define STDIN 0         // file descriptor for standard input
#define BUFSZ 131068    //Numero de bytes necessarios para receber todos os ids dos clientes de uma vez no CLIST.
#define OK 1
#define ERRO 2
#define OI 3
#define FLW 4
#define MSG 5
#define CREQ 6
#define CLIST 7
#define OIAP 13
#define MSGAP 15
#define CREQAP 16
#define CLISTAP 17
#define SERV 65535

uint16_t selfId;        //Identificador próprio do cliente.
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

int send_msg(uint16_t type, uint16_t idDest, uint16_t seq, char *txt, uint16_t length){
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

    if (type == MSG || type == MSGAP || type == OIAP) {
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

int get_txt(int mustRead, char *buf){
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
    int nClients;           //Numero de clientes retornado pelo CLIST/CLISTAP.
    uint16_t type;          //Tipo da mensagem recebida.
    uint16_t idOrig;        //ID do remetente da mensagem.
    uint16_t idDest;        //ID do destinatario da mensagem.
    uint16_t seq;           //Numero de sequencia da mensagem.
    char buf[BUFSZ];        //Buffer para fazer leitura do socket.
    char txt[500];          //String para armazenar o texto digitado pelo usuario.
    char apelido[30];       //String para armazenar o apelido digitado pelo usuario.
    char option;            //Opção escolhida pelo usuario.
    uint16_t counter = 1;   //Contador para o numero de sequencia da mensagem. Começa em um pois a mensagem 0 é a OI.
    uint16_t tamanhoMsg;    //Tamanho da mensagem a ser lida/escrita no socket.
    int portno = atoi(argv[2]);
    int nBytes;             //Numero de bytes lidos/escritos com sucesso. Serve para identificar erros na conexão.
    int seqFLW = -1;             //Armazena o numero de sequencia da mensagem de FLW para conferir quando o servidor responder um OK.


    //////////Configurando conexão.
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
    /////////////

    //Configurando select.
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
            fgets (buf, 500, stdin);
            switch (buf[0]){
                case 'M' :      //O usuario quer enviar uma mensagem com ID.
                    if (sscanf(buf, "%c %d %[^\n]s", &option, &idDest, txt) < 3) {
                        printf("Opção não reconhecida.\n Digite o ID do destinatário e a mensagem separados por um espaço.\n");
                        break;
                    }
                    if (send_msg(MSG, idDest, counter, txt, strlen(txt)) != 0)
                        return 0;
                    counter++;
                    break;
                case 'K' :      //O usuario quer enviar uma mensagem com apelido.
                    if (sscanf(buf, "%c %s %[^\n]s", &option, apelido, txt) < 3) {
                        printf("Opção não reconhecida.\n Digite o apelido do destinatário e a mensagem separados por um espaço.");
                        break;
                    }
                    //Envia header da mensagem, seguido do comprimento do apelido e o apelido em si.
                    if (send_msg(MSGAP, SERV, counter, apelido, strlen(apelido)) != 0)
                        return 0;

                    tamanhoMsg = htons(strlen(txt));
                    //Envia o tamanho do texto da mensagem.
                    if ((nBytes = send(sock, &tamanhoMsg, 2, MSG_NOSIGNAL)) < 2){
                        if (nBytes < 0) {
                            perror("send");
                        } else {
                            printf("Servidor não está respondendo corretamente.\n");
                        }
                        printf("Encerrando cliente.\n");
                        close(sock);
                        return 0;
                    }

                    //Envia o texto da mensagem.
                    if ((nBytes = send(sock, txt, strlen(txt), MSG_NOSIGNAL)) < strlen(txt)){
                        if (nBytes < 0) {
                            perror("send");
                        } else {
                            printf("Servidor não está respondendo corretamente.\n");
                        }
                        printf("Encerrando cliente.\n");
                        close(sock);
                        return 1;
                    }
                    counter++;
                    break;
                case 'L' :      //O usuario quer ver uma lista dos IDS conectados.
                    if(send_msg(CREQ, SERV, counter, "", 0) != 0)
                        return 0;
                    counter++;
                    break;
                case 'N' :      //O usuario quer ver uma lista dos apelidos conectados.
                    if(send_msg(CREQAP, SERV, counter, "", 0) != 0)
                        return 0;
                    counter++;
                    break;
                case 'S' :      //O usuario quer se desconectar.
                    if (send_msg(FLW, SERV, counter, "", 0) != 0)
                        return 0;
                    seqFLW = counter;
                    counter++;
                    break;
                case 'A' :      //O usuario quer fornecer um apelido.
                    if (sscanf(buf, "%c %s", &option, txt) < 2) {
                        printf("Opção não reconhecida.\n O apelido não pode ter espaços.\n");
                        break;
                    }
                    if (send_msg(OIAP, SERV, counter, txt, strlen(txt)) != 0)
                        return 0;
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
                        //Verifica se o cliente realmente pediu para se desconectar.
                        //Ignora todas as outras mensagens de OK.
                        if (seq == (uint16_t) seqFLW){
                            printf("Encerrando cliente.\n");
                            close(sock);
                            return 0;
                        }
                    break;
                //Ignora mensagens de ERRO.
                case ERRO :
                    break;
                case MSG :
                    //Pega tamanho do texto da mensagem.
                    if (get_txt(2, buf) != 0)
                        return 0;
                    tamanhoMsg = ntohs(*(uint16_t*) buf);

                    //Recebe texto da mensagem.
                    if (get_txt(tamanhoMsg, buf) != 0)
                        return 0;

                    //Verifica se a mensagem foi enviada para o destinatario incorreto.
                    if (selfId != idDest && idDest != 0){
                        if(send_msg(ERRO, idOrig, seq, "", 0) != 0)
                            return 0;
                    } else {    //Se o destinatário está correto, imprime na tela.
                        if (send_msg(OK, idOrig, seq, "", 0) != 0)
                            return 0;
                        buf[tamanhoMsg] = '\0';
                        printf("Mensagem de %d: %s\n", idOrig, buf);
                    }
                    break;
                case CLIST:
                    //Recebe quantos clientes estão online.
                    if (get_txt(2, buf) != 0)
                        return 0;
                    nClients = ntohs(*(uint16_t*) buf);

                    if (nClients > 0) {
                        //Recebe todos os IDs de todos os clientes de uma vez.
                        if (get_txt(2 * nClients, buf) != 0)
                            return 0;

                        if (send_msg(OK, idOrig, seq, "", 0) != 0)
                            return 0;
                        printf("Clientes disponíveis:\n");

                        //Imprime cada ID na tela.
                        for (int i = 0; i < nClients; i++)
                            printf("%d\n", ntohs(*(uint16_t*) (buf + 2 * i)));
                    } else {
                        printf("Nenhum cliente disponível.\n");
                    }                   

                    break;
                case CLISTAP:
                    //Recebe quantos clientes estão online.
                    if (get_txt(2, buf) != 0)
                        return 0;
                    nClients = ntohs(*(uint16_t*) buf);

                    if (nClients > 0) {
                    printf("Clientes disponíveis:\n");
                        for (int i = 0; i < nClients; i++) {
                            //Recebe ID do cliente.
                            if (get_txt(2, buf) != 0)
                                return 0;
                            printf("%d ", ntohs(*((uint16_t*) buf)));
                            //Recebe comprimento do apelido.
                            if (get_txt(2, buf) != 0)
                                return 0;
                            tamanhoMsg = ntohs(*((uint16_t*) buf));
                            //Recebe apelido.
                            if (tamanhoMsg > 0) {
                                if (get_txt(tamanhoMsg, buf) != 0)
                                    return 0;
                                buf[tamanhoMsg] = '\0';
                                printf("%s", buf);
                            }
                            printf("\n");
                        }
                    } else {
                        printf("Nenhum cliente disponível.\n");
                    }
                    if (send_msg(OK, idOrig, seq, "", 0) != 0)
                        return 0;
                    break;
                default :
                    //Tipo de mensagem não reconhecido.
                    if (send_msg(ERRO, idOrig, seq, "", 0) != 0)
                        return 0;
            }
        }
    }
}

