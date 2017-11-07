#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <sstream>

#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

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
#define OIAP 13
#define MSGAP 15
#define CREQAP 16
#define CLISTAP 17
#define SERV 65535

uint16_t selfId;        //Identificador do cliente.
int sock;               //Socket conectado ao servidor.

int get_header(uint16_t *type, uint16_t *idOrig, uint16_t *idDest, uint16_t *seq) {
	int nBytes;
	uint8_t buf[BUFSZ];
	if ((nBytes = recv(sock, buf, 8, MSG_WAITALL)) < 8){
		// got error or connection closed by server
		if (nBytes < 0) {
			perror("recv");
		} else{
			printf("Servidor nao esta respondendo corretamente.\n");
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
			printf("Servidor nao esta respondendo corretamente.\n");
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
				printf("Servidor nao esta respondendo corretamente.\n");
			}
			printf("Encerrando cliente.\n");
			close(sock);
			return 1;
		}

		if ((nBytes = send(sock, txt, length, MSG_NOSIGNAL)) < length){
			if (nBytes < 0) {
				perror("send");
			} else {
				printf("Servidor nao esta respondendo corretamente.\n");
			}
			printf("Encerrando cliente.\n");
			close(sock);
			return 1;
		}
	}

	return 0;
}

int inicia_cliente() {
	uint16_t type;
	uint16_t idOrig;
	uint16_t idDest;
	uint16_t seq;

	selfId = 0;     //O cliente ainda nao tem id proprio.
	if (send_msg(OI, SERV, 0, "", 0) != 0)
		return 1;

	if (get_header(&type, &idOrig, &idDest, &seq) != 0)
		return 1;

	if (type != OK){
		printf("Servidor nao esta respondendo corretamente.\n");
		printf("Encerrando cliente.\n");
		close(sock);
		return 1;
	}

	selfId = idDest;
	printf("Seu número identificador e: %d\n", selfId);
	return 0;
}

int get_txt(int mustRead, char *buf) {
	int nBytes;
	if ((nBytes = recv(sock, buf, mustRead, MSG_WAITALL)) < mustRead){
		// got error or connection closed by server
		if (nBytes < 0) {
			perror("recv");
		} else {
			// connection closed
			printf("Servidor nao esta respondendo corretamente.\n");
		}
		printf("Encerrando cliente.\n");
		close(sock);
		return 1;
	}
	return 0;
}

int main(int argc, char ** argv) {

	int nClients;   //Numero de clientes retornado pelo CLIST.
	uint16_t type;  //Tipo da mensagem recebida.
	uint16_t idOrig;    //ID do remetente da mensagem.
	uint16_t idDest;    //ID do destinatario da mensagem.
	uint16_t seq;       //Numero de sequencia da mensagem.
	char buf[BUFSZ]; //Buffer para fazer leitura do socket.
	char txt[500];      //String para armazenar o texto digitado pelo usuario.
	char apelido[30];      //String para armazenar o texto digitado pelo usuario.
	char option;        //Opcao escolhida pelo usuario.
	uint16_t counter = 1;   //Contador para o numero de sequencia da mensagem.
	uint16_t tamanhoMsg;
	int portno = atoi(argv[2]);
	int nBytes;

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

	//iniciar o SDL, TTF e configurar a fonte
	SDL_Init(SDL_INIT_EVERYTHING);
	TTF_Init();
	SDL_Color textColor = { 255, 255, 255, 0xFF };
	//tamanho da fonte em POINTS
	TTF_Font *font = TTF_OpenFont("VT323-Regular.ttf",20);
	TTF_Font *font2 = TTF_OpenFont("VT323-Regular.ttf",22);

	//a janela e o renderizador
	SDL_Window *win = SDL_CreateWindow("IRedesC", 0, 0, 700, 700,0);
	if (win == NULL) {
		perror("SDL_CreateWindow\n");
		SDL_Quit();
		return 1;
	}

	SDL_Renderer *ren = SDL_CreateRenderer(win,-1,0);
	if (ren == NULL) {
		SDL_DestroyWindow(win);
		perror("SDL_CreateRenderer\n");
		SDL_Quit();
		return 1;
	}

	//setando a imagem de background
	SDL_Surface *backgroundSurface = SDL_LoadBMP("client-background.bmp");
	SDL_Texture *backgroundTexture = SDL_CreateTextureFromSurface(ren,backgroundSurface);


	SDL_FreeSurface(backgroundSurface);


	string msg, log;

	fd_set readfds, master;         //Set de streams que vamos vigiar para leitura.
	FD_ZERO(&readfds);
	FD_ZERO(&master);
	FD_SET(STDIN, &master);
	FD_SET(sock, &master);        //Adiciona o socket ao set de leituras a ser observado.

	if (inicia_cliente() != 0)
		return 0;


	bool quit = false;

	//podemos começar a receber inputs
	SDL_StartTextInput();

	while (!quit) {

		//estamos aguardando algum evento na interface
		SDL_Event e;
		if (SDL_WaitEvent(&e)) {

			if (e.type == SDL_QUIT) {
				quit = true;


			} else if (e.type == 768) {

				//backspace
				if (e.key.keysym.sym == SDLK_BACKSPACE && msg.size()>0) {
					msg.erase(msg.size()-1,1);

				} else if (e.key.keysym.sym == SDLK_RETURN) {

					//quando o usuario aperta enter, entedemos que ele quer processar sua requisicao
	            switch (msg.c_str()[0]) {
						case 'M' :      //O usuario quer enviar uma mensagem com ID.
							if (sscanf(msg.c_str(), "%c %d %[^\n]s", &option, &idDest, txt) < 3) {
								log = string("Opcao nao reconhecida.\n Digite o ID do destinatario e a mensagem separados por um espaco.\n");
								break;
							}
							if (send_msg(MSG, idDest, counter, txt, strlen(txt)) != 0)
								return 0;
							counter++;
							break;

						case 'K' :      //O usuario quer enviar uma mensagem com apelido.
							if (sscanf(msg.c_str(), "%c %s %[^\n]s", &option, apelido, txt) < 3) {
								log = string("Opcao nao reconhecida.\n Digite o apelido do destinatario e a mensagem separados por um espaco.");
								break;
							}
							if (send_msg(MSGAP, SERV, counter, apelido, strlen(apelido)) != 0)
								return 0;
							tamanhoMsg = htons(strlen(txt));
							if ((nBytes = send(sock, &tamanhoMsg, 2, MSG_NOSIGNAL)) < 2) {
								if (nBytes < 0) {
									perror("send");
								} else {
									log = string("Servidor nao esta respondendo corretamente.\n");
							}
							log += string("Encerrando cliente.\n");
							close(sock);
							quit = true;
							}
							if ((nBytes = send(sock, txt, strlen(txt), MSG_NOSIGNAL)) < strlen(txt)) {
								if (nBytes < 0) {
									perror("send");
								} else {
									log = string("Servidor nao esta respondendo corretamente.\n");
								}
								log = string("Encerrando cliente.\n");
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
								if (sscanf(msg.c_str(), "%c %s", &option, txt) < 2) {
								log = string("Opcao nao reconhecida.\n O apelido nao pode ter espacos.\n");
								break;
								}
								if (send_msg(OIAP, SERV, counter, txt, strlen(txt)) != 0)
									return 0;
									counter++;
									break;
							default :
								log = string("Opcao nao reconhecida.\n Tecle M para enviar uma mensagem.\n Tecle L para listar os destinatarios disponiveis.\n Ou tecle S para sair.\n");
					}
					msg.clear();
				}

			} else if (e.type == SDL_TEXTINPUT) {
				msg += string(e.text.text);
				if (msg.size()>300) {
					msg.erase(msg.size()-1,msg.size()-300);
				}
			}
		}

		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 1;

		readfds = master; // copy it
		if (select(sock+1,&readfds,NULL,NULL,&tv) == -1){ //Observa o set readfds.
			close(sock);
			perror("select");
			log = string("Encerrando cliente.\n");
			quit = true;
		}

		if (FD_ISSET(sock, &readfds)) {
			if (get_header(&type, &idOrig, &idDest, &seq) != 0)
				return 0;

			switch (type){
				case OK :
					if (seqFLW != -1)
						if (seq == (uint16_t) seqFLW){
							log = string("Encerrando cliente.\n");
							close(sock);
							quit = true;
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
						buf[tamanhoMsg] = '\0';
						log = string("Mensagem de ") + SSTR(idOrig) + SSTR(": ") + string(buf) + string("\n");
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
					log = string("Clientes disponiveis:\n");

					for (int i = 0; i < nClients; i++)
						log += SSTR(ntohs(*(uint16_t*) (buf + 2 * i))) + string("\n");

					break;

				case CLISTAP:
					if (get_txt(2, buf) != 0)
						return 0;

					nClients = ntohs(*(uint16_t*) buf);
					log = string("Clientes disponiveis:\n");
					for (int i = 0; i < nClients; i++) {
						//ID do cliente.
						if (get_txt(2, buf) != 0)
							return 0;
						log += SSTR(ntohs(*((uint16_t*) buf))) + string(" ");
						if (get_txt(2, buf) != 0)
							return 0;
						tamanhoMsg = ntohs(*((uint16_t*) buf));
						if (tamanhoMsg > 0) {
							if (get_txt(tamanhoMsg, buf) != 0)
								return 0;
							buf[tamanhoMsg] = '\n';
							log += string(buf);
						}

						
					}
					if (send_msg(OK, idOrig, seq, "", 0) != 0)
						return 0;
					break;
				default :
					if (send_msg(ERRO, idOrig, seq, "", 0) != 0)
						return 0;
			}
		}

		//o lugar em que o usuario digita requisicoes
		SDL_Surface *termSurface = TTF_RenderText_Blended_Wrapped(font,msg.c_str(),textColor,330);
		SDL_Texture *termTexture = SDL_CreateTextureFromSurface(ren,termSurface);

		//o lugar em que aparece a resposta a sua requisicao e tambem mensagens
		SDL_Surface *logSurface = TTF_RenderText_Blended_Wrapped(font,log.c_str(),textColor,330);
		SDL_Texture *logTexture = SDL_CreateTextureFromSurface(ren,logSurface);

		//apenas para mostrar o numero do cliente na interface
		SDL_Surface *clientIDSur = TTF_RenderText_Blended_Wrapped(font2,SSTR(selfId).c_str(),textColor,10);
		SDL_Texture *clientIDTex = SDL_CreateTextureFromSurface(ren,clientIDSur);

		//calculamos a superficie do texto
		int wT, hT, wL, hL, w2, h2;
		TTF_SizeUTF8(font,msg.c_str(),&wT,&hT);
		TTF_SizeUTF8(font,log.c_str(),&wL,&hL);
		TTF_SizeUTF8(font2,SSTR(selfId).c_str(),&w2,&h2);
		
		//acessamos os atributos das texturas
		SDL_QueryTexture(termTexture,NULL,NULL,&wT,&hT);
		SDL_QueryTexture(logTexture,NULL,NULL,&wL,&hL);
		SDL_QueryTexture(clientIDTex,NULL,NULL,&w2,&h2);

		SDL_FreeSurface(termSurface);
		SDL_FreeSurface(logSurface);
		SDL_FreeSurface(clientIDSur);

		//criamos a area onde serao apresentados os textos
		SDL_Rect clientTerminal = {100, 70, wT, hT};
		SDL_Rect logTerminal = {100, 250, wL, hL};
		SDL_Rect clientID = {190, 540, w2, h2};

		//limpando o renderizador
		SDL_RenderClear(ren);

		//processando nossas texturas: background, terminal do usuario, respostas, exibicao da id
		SDL_RenderCopy(ren,backgroundTexture,NULL,NULL);
		SDL_RenderCopy(ren,termTexture,NULL,&clientTerminal);
		SDL_RenderCopy(ren,logTexture,NULL,&logTerminal);
		SDL_RenderCopy(ren,clientIDTex,NULL,&clientID);
		SDL_RenderPresent(ren);


		SDL_DestroyTexture(termTexture);
		SDL_DestroyTexture(logTexture);
		SDL_DestroyTexture(clientIDTex);
		
	}

	SDL_StopTextInput();


	SDL_DestroyTexture(backgroundTexture);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);

	TTF_CloseFont(font);
	TTF_CloseFont(font2);
	TTF_Quit();
	SDL_Quit();

	return 0;
}