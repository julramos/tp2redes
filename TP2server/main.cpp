#include <iostream>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <map>

using namespace std;

#define BUFSZ 410
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


struct SocksIds {       //Permite recuperar o id de um socket.
    int sock;
    string apelido;
    uint16_t id;
};
fd_set master;    // master file descriptor list
vector<SocksIds> connections;
vector<SocksIds>::iterator it;
map<string, uint16_t> apelidosEIds;
int onlineClients[65535]; //0 se o id nao pertence a um cliente conectado, caso contrario o numero do seu socket. Permite recuperar o socket de um id.
queue<uint16_t> availableIds; //Fila para armazenar os ids disponvieis.

void trataErro() {
    if (it->id != 0) {  //Checa se o cliente ja recebeu um ID.
        onlineClients[it->id] = 0;
        availableIds.push(it->id);
    }
    if (it->apelido != "") {    //Checa se ele tem um apelido.
        apelidosEIds.erase(apelidosEIds.find(it->apelido));
    }
    close(it->sock);
    FD_CLR(it->sock, &master);
    connections.erase(it);
    --it;
}

int send_msg(uint16_t type, uint16_t idDest, uint16_t idOrig, uint16_t seq, char *txt, int length, int flag){
    uint8_t msg[BUFSZ];
    int nBytes;
    ((uint16_t*)msg)[0] = htons(type);
    ((uint16_t*)msg)[1] = htons(idOrig);
    ((uint16_t*)msg)[2] = htons(idDest);
    ((uint16_t*)msg)[3] = htons(seq);

    if ((nBytes = send(it->sock, msg, 8, MSG_NOSIGNAL)) < 8){
        if (nBytes < 0) {
            perror("send");
        }
        //Cliente não está respondendo corretamente.
        if (flag)
            trataErro();
        return 1;
    }

    if (type == MSG) {
        ((uint16_t*)msg)[0] = htons(length);
        if ((nBytes = send(it->sock, msg, 2, MSG_NOSIGNAL)) < 2){
            if (nBytes < 0) {
                perror("send");
            }
            //Cliente não está respondendo corretamente.
            if (flag)
                trataErro();
            return 1;
        }

        if ((nBytes = send(it->sock, txt, length, MSG_NOSIGNAL)) < length){
            if (nBytes < 0) {
                perror("send");
            }
            //Cliente não está respondendo corretamente.
            if (flag)
                trataErro();
            return 1;
        }
    }
    return 0;
}

int enviaLista(uint16_t idDest, uint16_t seq) {
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

    if ((nBytes = send(it->sock, msg, 10, MSG_NOSIGNAL)) < 10){
        if (nBytes < 0) {
            perror("send");
        }
        //Cliente não está respondendo corretamente.
        trataErro();
        return 1;
    }

    if ((nBytes = send(it->sock, (uint8_t *) listOfIDs, total * 2, MSG_NOSIGNAL)) < total * 2){
        if (nBytes < 0) {
            perror("send");
        }
        //Cliente não está respondendo corretamente.
        trataErro();
        return 1;
    }

    return 0;
}

int enviaListaAp(uint16_t idDest, uint16_t seq) {
    uint8_t msg[10];
    int nBytes;
    uint16_t tamanho;
    uint16_t id;

    ((uint16_t*)msg)[0] = htons(CLISTAP);
    ((uint16_t*)msg)[1] = htons(SERV);
    ((uint16_t*)msg)[2] = htons(idDest);
    ((uint16_t*)msg)[3] = htons(seq);
    ((uint16_t*)msg)[4] = htons(connections.size());

    if ((nBytes = send(it->sock, msg, 10, MSG_NOSIGNAL)) < 10){
        if (nBytes < 0) {
            perror("send");
        }
        //Cliente não está respondendo corretamente.
        trataErro();
        return 1;
    }

    vector<SocksIds>::iterator temp;
    for (temp = connections.begin(); temp != connections.end(); ++temp) {
        //ID.
        id = htons(temp->id);
        if ((nBytes = send(it->sock, &id, 2, MSG_NOSIGNAL)) < 2){
            if (nBytes < 0) {
                perror("send");
            }
            //Cliente não está respondendo corretamente.
            trataErro();
            return 1;
        }
        if (temp->apelido != "") {
            tamanho = htons(temp->apelido.size());
            //Tamanho do apelido.
            if ((nBytes = send(it->sock, &tamanho, 2, MSG_NOSIGNAL)) < 2){
                if (nBytes < 0) {
                    perror("send");
                }
                //Cliente não está respondendo corretamente.
                trataErro();
                return 1;
            }
            if ((nBytes = send(it->sock, temp->apelido.c_str(), temp->apelido.size(), MSG_NOSIGNAL)) < temp->apelido.size()){
                if (nBytes < 0) {
                    perror("send");
                }
                //Cliente não está respondendo corretamente.
                trataErro();
                return 1;
            }
        } else {
            tamanho = 0;
            //Tamanho do apelido.
            if ((nBytes = send(it->sock, &tamanho, 2, MSG_NOSIGNAL)) < 2){
                if (nBytes < 0) {
                    perror("send");
                }
                //Cliente não está respondendo corretamente.
                trataErro();
                return 1;
            }
        }
    }
    return 0;
}

int get_header(uint16_t *type, uint16_t *idOrig, uint16_t *idDest, uint16_t *seq){
    int nBytes;
    uint8_t buf[BUFSZ];
    if ((nBytes = recv(it->sock, buf, 8, MSG_WAITALL)) < 8){
        // got error or connection closed by server
        if (nBytes < 0) {
            perror("recv");
        }
        //Cliente não está respondendo corretamente.
        trataErro();
        return 1;
    }
    *type = ntohs(*(uint16_t*) buf);
    *idOrig = ntohs(*(uint16_t*) (buf + 2));
    *idDest = ntohs(*(uint16_t*) (buf + 4));
    *seq = ntohs(*(uint16_t*) (buf + 6));
    return 0;
}

int get_txt(int mustRead, char *buf){
    int nBytes;

    if ((nBytes = recv(it->sock, buf, mustRead, MSG_WAITALL)) < mustRead){
        // got error or connection closed by server
        if (nBytes < 0) {
            perror("recv");
        }
        //Cliente não está respondendo corretamente.
        trataErro();
        return 1;
    }
    return 0;
}

void close_all(vector<SocksIds> connections){
    for (vector<SocksIds>::iterator i = connections.begin() ; i != connections.end(); ++i) {
        close(i->sock);
    }
}

int main(int argc, char *argv[])
{
    SocksIds aux;
    uint16_t type;  //Tipo da mensagem recebida.
    uint16_t idOrig;    //ID do remetente da mensagem.
    uint16_t idDest;    //ID do destinatario da mensagem.
    uint16_t seq;       //Numero de sequencia da mensagem.
    char buf[BUFSZ]; //Buffer para fazer leitura do socket.
    uint16_t tamanhoMsg;
    uint16_t newClient;         //Armazena temporariamente o id de um cliente que acabou de se conextar.
    char apelido[30];
    char txt[401];
    map<string,uint16_t>::iterator pointer;
    vector<SocksIds>::iterator temp;    //Para nao perder o it, quando for necessario enviar msg para outro cliente.

    memset(onlineClients, 0, 65535 * sizeof(int));

    for (int i = 1; i < 65535; i++)
        availableIds.push(i);


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
                setsockopt(newsockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));  //Seta o tempo de timeout.
                aux.id = 0;
                aux.sock = newsockfd;
                aux.apelido = "";
                connections.push_back(aux);
                FD_SET(newsockfd, &master); // add to master set
                if (newsockfd > fdmax) {    // keep track of the max
                    fdmax = newsockfd;
                }
            }
        }

        // run through the existing connections looking for data to read
        for (it = connections.begin(); it != connections.end(); ++it) {
            if (FD_ISSET(it->sock, &read_fds)) {
                if (get_header(&type, &idOrig, &idDest, &seq) != 0)
                    continue;

                if (idOrig != it->id){    //Checa se id enviado coincide com o remetente.
                    send_msg(ERRO, it->id, SERV, seq, "", 0, 1);
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
                            if (send_msg(OK, newClient, SERV, seq, "", 0, 1) != 0)
                                break;
                            availableIds.pop();
                            it->id = newClient;
                            onlineClients[newClient] = it->sock;
                        } else
                            send_msg(ERRO, it->id, SERV, seq, "", 0, 1);
                        break;
                    case OIAP :
                        if (it->id == 0) {          //Checa se o cliente nao tem um ID.
                            newClient = availableIds.front();
                            if (send_msg(OK, newClient, SERV, seq, "", 0, 1) != 0)
                                break;
                            availableIds.pop();
                            it->id = newClient;
                            onlineClients[newClient] = it->sock;
                        }
                        if (it->apelido == "") {     //Checa se o cliente nao tem apelido.
                            //Recebe quantos caracteres tem no apelido.
                            if (get_txt(2, buf) != 0)
                                break;

                            tamanhoMsg = ntohs(*(uint16_t*) buf);

                            //Pega apelido.
                            if (get_txt(tamanhoMsg, apelido) != 0)
                                break;

                            apelido[tamanhoMsg] = '\0';
                            pointer = apelidosEIds.find(apelido);
                            if (pointer == apelidosEIds.end()) {   //Verifica se o apelido ja foi usado.
                                it->apelido = apelido;
                                apelidosEIds[apelido] = it->id;
                            } else {
                                send_msg(ERRO, it->id, SERV, seq, "", 0, 1);
                            }
                        } else
                            send_msg(ERRO, it->id, SERV, seq, "", 0, 1);
                        break;
                    case FLW :
                        if (send_msg(OK, it->id, SERV, seq, "", 0, 1) == 0) {
                            trataErro();    //So aproveitando a funcao pra fechar tudo certinho.
                                            //Se a msg de OK tiver dado erro, ela ja vai ter fechado.
                        }
                        break;
                    case MSG :
                        //Recebe quantos caracteres tem no texto.
                        if (get_txt(2, buf) != 0)
                            break;

                        tamanhoMsg = ntohs(*(uint16_t*) buf);

                        if (get_txt(tamanhoMsg, buf) != 0)
                            break;

                        if (idDest == 0) {  //Broadcast
                            if (send_msg(OK, idOrig, SERV, seq, "", 0, 1) != 0)
                                break;

                            temp = it;
                            for (it = connections.begin(); it != connections.end(); ++it)
                                if (it->id != idOrig)
                                    send_msg(MSG, 0, idOrig, seq, (char *) buf, tamanhoMsg, 1);

                            it = temp;
                            break;
                        }

                        if (onlineClients[idDest] != 0){    //Checa se destino esta conectado.
                            if (send_msg(OK, idOrig, SERV, seq, "", 0, 1) != 0)
                                break;

                            int s = it->sock;
                            it->sock = onlineClients[idDest];
                            if (send_msg(MSG, idDest, idOrig, seq, (char *) buf, tamanhoMsg, 0) != 0) {
                                //Procura o socket na lista de conexões.
                                temp = it;
                                for (it = connections.begin(); it != connections.end(); ++it)
                                    if (it->id == idDest) {
                                        trataErro();
                                        break;
                                    }
                                it = temp;
                            }
                            it->sock = s;
                            break;
                        }
                        send_msg(ERRO, idOrig, SERV, seq, "", 0, 1);
                        break;
                    case MSGAP :
                        //Recebe quantos caracteres tem no apelido.
                        if (get_txt(2, buf) != 0)
                            break;
                        tamanhoMsg = ntohs(*(uint16_t*) buf);
                        //Pega apelido.
                        if (get_txt(tamanhoMsg, apelido) != 0)
                            break;
                        apelido[tamanhoMsg] = '\0';


                        //Recebe quantos caracteres tem na mensagem.
                        if (get_txt(2, buf) != 0)
                            break;
                        tamanhoMsg = ntohs(*(uint16_t*) buf);
                        //Pega mensagem.
                        if (get_txt(tamanhoMsg, txt) != 0)
                            break;
                        txt[tamanhoMsg] = '\0';

                        pointer = apelidosEIds.find(apelido);
                        if (pointer != apelidosEIds.end()){    //Checa se destino esta conectado.
                            if (send_msg(OK, idOrig, SERV, seq, "", 0, 1) != 0)
                                break;
                            idDest = pointer->second;

                            int s = it->sock;
                            it->sock = onlineClients[idDest];
                            if (send_msg(MSG, idDest, idOrig, seq, txt, strlen(txt), 0) != 0) {
                                //Procura o socket na lista de conexões.
                                temp = it;
                                for (it = connections.begin(); it != connections.end(); ++it)
                                    if (it->id == idDest) {
                                        trataErro();
                                        break;
                                    }
                                it = temp;
                            }
                            it->sock = s;
                            break;
                        }
                        send_msg(ERRO, idOrig, SERV, seq, "", 0, 1);
                        break;
                    case CREQ:
                        enviaLista(idOrig, seq);
                        break;
                    case CREQAP:
                        enviaListaAp(idOrig, seq);
                        break;
                    default :
                        send_msg(ERRO, idOrig, SERV, seq, "", 0, 1);
                }
            }
        }
    }
}
