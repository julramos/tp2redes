#include <iostream>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <queue>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

#define BUFSZ 410
#define OK 1
#define ERRO 2
#define OI 3
#define FLW 4
#define MSG 5
#define CREQ 6
#define CLIST 7
#define SERV 65535

int sock;

int send_msg(uint16_t type, uint16_t idDest, uint16_t idOrig, uint16_t seq, char *txt, int length){
    uint8_t msg[BUFSZ];
    int nBytes;
    ((uint16_t*)msg)[0] = htons(type);
    ((uint16_t*)msg)[1] = htons(idOrig);
    ((uint16_t*)msg)[2] = htons(idDest);
    ((uint16_t*)msg)[3] = htons(seq);

    if ((nBytes = send(sock, msg, 8, MSG_NOSIGNAL)) < 8){
        if (nBytes < 0) {
            perror("send");
        }
        //Cliente não está respondendo corretamente.
        close(sock);
        return 1;
    }

    if (type == MSG) {
        ((uint16_t*)msg)[0] = htons(length);
        if ((nBytes = send(sock, msg, 2, MSG_NOSIGNAL)) < 2){
            if (nBytes < 0) {
                perror("send");
            }
            //Cliente não está respondendo corretamente.
            close(sock);
            return 1;
        }

        if ((nBytes = send(sock, txt, length, MSG_NOSIGNAL)) < length){
            if (nBytes < 0) {
                perror("send");
            }
            //Cliente não está respondendo corretamente.
            close(sock);
            return 1;
        }
    }
    return 0;
}

int enviaLista(uint16_t idDest, uint16_t seq, int *onlineClients) {
    uint8_t msg[10];
    uint16_t listOfIDs[65535];
    uint16_t total = 0;
    int nBytes;

    for (int i = 1; i < SERV; i++) {
        if (onlineClients[i] != 0) {
            listOfIDs[total] = htons(i);
            total++;
        }
    }

    ((uint16_t*)msg)[0] = htons(CLIST);
    ((uint16_t*)msg)[1] = htons(SERV);
    ((uint16_t*)msg)[2] = htons(idDest);
    ((uint16_t*)msg)[3] = htons(seq);
    ((uint16_t*)msg)[4] = htons(total);

    if ((nBytes = send(sock, msg, 10, MSG_NOSIGNAL)) < 10){
        if (nBytes < 0) {
            perror("send");
        }
        //Cliente não está respondendo corretamente.
        close(sock);
        return 1;
    }

    if ((nBytes = send(sock, (uint8_t *) listOfIDs, total * 2, MSG_NOSIGNAL)) < total * 2){
        if (nBytes < 0) {
            perror("send");
        }
        //Cliente não está respondendo corretamente.
        close(sock);
        return 1;
    }

    return 0;
}

int get_header(uint16_t *type, uint16_t *idOrig, uint16_t *idDest, uint16_t *seq){
    int nBytes;
    uint8_t buf[BUFSZ];
    if ((nBytes = recv(sock, buf, 8, MSG_WAITALL)) < 8){
        // got error or connection closed by server
        if (nBytes < 0) {
            perror("recv");
        }
        //Cliente não está respondendo corretamente.
        close(sock);
        return 1;
    }
    *type = ntohs(*(uint16_t*) buf);
    *idOrig = ntohs(*(uint16_t*) (buf + 2));
    *idDest = ntohs(*(uint16_t*) (buf + 4));
    *seq = ntohs(*(uint16_t*) (buf + 6));
    return 0;
}

int get_txt(int mustRead, uint8_t *buf){
    int nBytes;

    if ((nBytes = recv(sock, buf, mustRead, MSG_WAITALL)) < mustRead){
        // got error or connection closed by server
        if (nBytes < 0) {
            perror("recv");
        }
        //Cliente não está respondendo corretamente.
        close(sock);
        return 1;
    }
    return 0;
}

struct SocksIds {       //Permite recuperar o id de um socket.
    int sock;
    uint16_t id;
};

void close_all(vector<SocksIds> connections){
    for (vector<SocksIds>::iterator it = connections.begin() ; it != connections.end(); ++it) {
        close(it->sock);
    }
}

int main(int argc, char *argv[])
{
    SocksIds temp;
    uint16_t type;  //Tipo da mensagem recebida.
    uint16_t idOrig;    //ID do remetente da mensagem.
    uint16_t idDest;    //ID do destinatario da mensagem.
    uint16_t seq;       //Numero de sequencia da mensagem.
    uint8_t buf[BUFSZ]; //Buffer para fazer leitura do socket.
    int onlineClients[65535]; //0 se o id nao pertence a um cliente conectado, caso contrario o numero do seu socket. Permite recuperar o socket de um id.
    uint16_t tamanhoMsg;
    vector<SocksIds> connections;
    queue<uint16_t> availableIds; //Fila para armazenar os ids disponvieis.
    uint16_t newClient;         //Armazena temporariamente o id de um cliente que acabou de se conextar.

    memset(onlineClients, 0, 65535 * sizeof(int));

    for (int i = 1; i < 65535; i++){
        availableIds.push(i);
    }


    /////////Inicializando servidor
    //struct para definir o timeout de 5 segundos
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    int listener, newsockfd;
    int portno = atoi(argv[1]);

    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    listener = socket(AF_INET, SOCK_STREAM, 0);

    //Reuso da porta.
    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(listener, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        perror("bind");
        return 0;
    }
    if (listen(listener, 5) < 0) {
       perror("listen");
       return 0;
    }
    clilen = sizeof(cli_addr);
    ////////////

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

     // add the listener to the master set
    FD_SET(listener, &master);
    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one


    while(1){
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            close_all(connections);
            close(listener);
            return 0;
        }

        if (FD_ISSET(listener, &read_fds)) {
            // handle new connections
            if ((newsockfd = accept(listener, (struct sockaddr * ) & cli_addr, & clilen)) < 0){
                perror("accept");
            } else {
                //setsockopt(newsockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));  //Seta o tempo de timeout.
                temp.id = 0;
                temp.sock = newsockfd;
                connections.push_back(temp);
                FD_SET(newsockfd, &master); // add to master set
                if (newsockfd > fdmax) {    // keep track of the max
                    fdmax = newsockfd;
                }
            }
        }

        // run through the existing connections looking for data to read
        for (vector<SocksIds>::iterator it = connections.begin() ; it != connections.end(); ++it) {
            if (FD_ISSET(it->sock, &read_fds)) {
                sock = it->sock;
                if (get_header(&type, &idOrig, &idDest, &seq) != 0) {
                    FD_CLR(sock, &master);
                    if (it->id != 0) {  //Checa se o cliente ja recebeu um ID.
                        availableIds.push(it->id);
                        onlineClients[it->id] = 0;
                    }
                    connections.erase(it);
                    --it;
                    continue;
                }

                if (idOrig != it->id){    //Checa se id enviado coincide com o remetente.
                    if (send_msg(ERRO, it->id, SERV, seq, "", 0) != 0) {
                        FD_CLR(sock, &master);
                        if (it->id != 0) {  //Checa se o cliente ja recebeu um ID.
                            availableIds.push(it->id);
                            onlineClients[it->id] = 0;
                        }
                        connections.erase(it);
                        --it;
                    }
                    continue;
                }

                switch (type){
                    case OK :
                        break;
                    case ERRO :
                        break;
                    case OI :
                        if (it->id == 0) {          //Checa se o cliente nao tem um ID.
                            newClient = availableIds.front();
                            if (send_msg(OK, newClient, SERV, seq, "", 0) != 0) {
                                connections.erase(it);
                                --it;
                                FD_CLR(sock, &master);
                            } else {
                                availableIds.pop();
                                it->id = newClient;
                                onlineClients[newClient] = sock;
                            }
                        } else {
                            send_msg(ERRO, it->id, SERV, seq, "", 0);
                        }
                        break;
                    case FLW :
                        if (it->id != 0) {  //Checa se o cliente ja recebeu um ID.
                            onlineClients[idOrig] = 0;
                            availableIds.push(idOrig);
                        }
                        send_msg(OK, it->id, SERV, seq, "", 0);
                        FD_CLR(sock, &master);
                        connections.erase(it);
                        --it;
                        close(sock);
                        break;
                    case MSG :
                        //Recebe quantos caracteres tem no texto.
                        if (get_txt(2, buf) != 0){
                            if (it->id != 0) {  //Checa se o cliente ja recebeu um ID.
                                onlineClients[idOrig] = 0;
                                availableIds.push(idOrig);
                            }
                            FD_CLR(sock, &master);
                            connections.erase(it);
                            --it;
                            break;
                        }
                        tamanhoMsg = ntohs(*(uint16_t*) buf);

                        if (get_txt(tamanhoMsg, buf) != 0){
                            if (it->id != 0) {  //Checa se o cliente ja recebeu um ID.
                                onlineClients[idOrig] = 0;
                                availableIds.push(idOrig);
                            }
                            FD_CLR(sock, &master);
                            connections.erase(it);
                            --it;
                            break;
                        }

                        if (idDest == 0) {  //Broadcast
                            if (send_msg(OK, idOrig, SERV, seq, "", 0) != 0) {
                                if (it->id != 0) {  //Checa se o cliente ja recebeu um ID.
                                    onlineClients[idOrig] = 0;
                                    availableIds.push(idOrig);
                                }
                                FD_CLR(sock, &master);
                                connections.erase(it);
                                --it;
                                break;
                            }
                            for (vector<SocksIds>::iterator jt = connections.begin() ; jt != connections.end(); ++jt) {
                                sock = jt->sock;
                                if (send_msg(MSG, 0, idOrig, seq, (char *) buf, tamanhoMsg) != 0) {
                                    if (jt->id != 0) {  //Checa se o cliente ja recebeu um ID.
                                        onlineClients[jt->id] = 0;
                                        availableIds.push(jt->id);
                                    }
                                    FD_CLR(sock, &master);
                                    connections.erase(jt);
                                    --jt;
                                }
                            }
                            break;
                        }
                        if (onlineClients[idDest] != 0){    //Checa se destino esta conectado.
                            if (send_msg(OK, idOrig, SERV, seq, "", 0) != 0) {
                                if (it->id != 0) {  //Checa se o cliente ja recebeu um ID.
                                    onlineClients[idOrig] = 0;
                                    availableIds.push(idOrig);
                                }
                                FD_CLR(sock, &master);
                                connections.erase(it);
                                --it;
                                break;
                            }
                            sock = onlineClients[idDest];
                            if (send_msg(MSG, idDest, idOrig, seq, (char *) buf, tamanhoMsg) != 0) {
                                onlineClients[idDest] = 0;
                                availableIds.push(idDest);
                                FD_CLR(sock, &master);
                                //Procura o socket na lista de conexões.
                                for (vector<SocksIds>::iterator jt = connections.begin() ; jt != connections.end(); ++jt)
                                    if (jt->id == idDest) {
                                        connections.erase(jt);
                                        --jt;
                                    }
                            }
                            break;
                        }
                        if (send_msg(ERRO, idOrig, SERV, seq, "", 0) != 0) {
                            if (it->id != 0) {  //Checa se o cliente ja recebeu um ID.
                                onlineClients[idOrig] = 0;
                                availableIds.push(idOrig);
                            }
                            FD_CLR(sock, &master);
                            connections.erase(it);
                            --it;
                        }
                        break;
                    case CREQ:
                        if (enviaLista(idOrig, seq, onlineClients) != 0) {
                            if (it->id != 0) {  //Checa se o cliente ja recebeu um ID.
                                onlineClients[idOrig] = 0;
                                availableIds.push(idOrig);
                            }
                            FD_CLR(sock, &master);
                            connections.erase(it);
                            --it;
                        }
                        break;
                    default :
                        if (send_msg(ERRO, idOrig, SERV, seq, "", 0) != 0) {
                            if (it->id != 0) {  //Checa se o cliente ja recebeu um ID.
                                onlineClients[idOrig] = 0;
                                availableIds.push(idOrig);
                            }
                            FD_CLR(sock, &master);
                            connections.erase(it);
                            --it;
                        }
                }
            }
        }
    }
}
