#include "util.h"


PEDIDO pedido;
char nome_fifo_local[25], fifo_jogador_sv[25];
char comando_local_separado[3][25];
int pid_servidor, c_equipa, c_jogador;
int resultado_esquerda = 0, resultado_direita = 0;
bool ha_jogo, dentro_jogo;
JOGADOR jogadores[TAM_JOGADORES];

WINDOW *win_consola, *win_info;

pthread_t t_campo;


/* Funcao que é chamda sempre que se recebe um sinal */
void trata_sinais(int sinal, siginfo_t *valor, void *n);

/* Separa o comando dado pelo cliente por espaços */
void separaComandos(char str[]);

/* Impressao formatada de um log */
void wprintlog(WINDOW *win, char str[]);

/* Impressao formatada de um erro */
void wprinterror(WINDOW *win, char str[]);

/* Conjunto de instrucoes para terminar o clientes */
void terminaCliente();

/* Funcao para desenhar o campo */
void desenhaCampo(WINDOW *win);

/* Funcao que se dedica a controlar o modo de jogo do cliente s*/
void *modoJogo();

/* Atualiza o score do jogo */
void *atualizaInfo();



int main(int argc, char *argv[]){
  	int ecra_y, ecra_x, i, fd_fifo, resp_login, aux, tecla;
  	bool flag_aux;

	/*
		Inicializa o tratamento de sinais
	*/
	struct sigaction sinal;
	sigemptyset(&sinal.sa_mask);
	sinal.sa_sigaction = trata_sinais;
	sinal.sa_flags = SA_SIGINFO;

	sigaction(SIGINT, &sinal, NULL);
	sigaction(SIGUSR1, &sinal, NULL);
	sigaction(SIGUSR2, &sinal, NULL);
	sigaction(SIGHUP, &sinal, NULL);
	sigaction(SIGPIPE, &sinal, NULL);
	sigaction(SIGURG, &sinal, NULL);

  	/*
    	Inicializa o ambiente ncurses
  	*/
	initscr();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLUE);
  	init_pair(2, COLOR_RED, COLOR_BLUE);
  	init_pair(3, COLOR_GREEN, COLOR_BLUE);
  	//Cores a utilizar para o campo
  	init_color(COLOR_GREEN, 0, 255, 0);
	init_pair(4, COLOR_WHITE, COLOR_GREEN);
	init_pair(5, COLOR_BLUE, COLOR_GREEN);
	init_pair(6, COLOR_RED, COLOR_GREEN);
	init_pair(7, COLOR_YELLOW, COLOR_GREEN);
	init_pair(8, COLOR_WHITE, COLOR_BLACK);

	clear();
	cbreak();

  	/*
    	Configuração da janela de trabalho.
  	*/
	getmaxyx(stdscr, ecra_y, ecra_x);
	win_consola = newwin(ecra_y, ecra_x, 0, 0);
	scrollok(win_consola, true);
  	wbkgd(win_consola, COLOR_PAIR(1));
	keypad(win_consola, TRUE);


	union sigval valor_sinal_main;

  	if(access(NOME_FIFO_SERVIDOR, F_OK) != 0){
    	delwin(win_consola);
    	endwin();
    	printf("[ERRO] O servidor nao esta a correr!\n");
    	return EXIT_SUCCESS;
  	}else{
  		pedido.user.pid = getpid();
  		pedido.user.logado = false;
  		dentro_jogo = false;

    	sprintf(nome_fifo_local, "fifo%d", pedido.user.pid); //Escreve para uma string
		sprintf(fifo_jogador_sv, "sv%d", getpid());
    	mkfifo(nome_fifo_local, 0600);
  	
    	fd_fifo=open(NOME_FIFO_SERVIDOR, O_WRONLY);
    	strcpy(pedido.comando, "ola");
		write(fd_fifo, &pedido, sizeof(pedido));
		close(fd_fifo);
		do{
			if(ha_jogo && dentro_jogo && pedido.user.logado){
				mkfifo(fifo_jogador_sv, 0600);
				strcpy(pedido.comando, "jogo");
				do{
					tecla = wgetch(win_consola);
					fd_fifo=open(fifo_jogador_sv, O_WRONLY);
					write(fd_fifo, &tecla, sizeof(tecla));
					close(fd_fifo);
				}while(tecla != 'q' && ha_jogo && dentro_jogo && pedido.user.logado);
				if(tecla == 'q'){
					valor_sinal_main.sival_int = 0;
					sigqueue(pid_servidor, SIGHUP, valor_sinal_main);
				}
			}else{
				wscanw(win_consola, "%79[^\n]", pedido.comando);
				separaComandos(pedido.comando);
			}
			if(!strcmp(comando_local_separado[0], "login")){
			  	if(pedido.user.logado){
			   		wprinterror(win_consola, "Ja se encontra logado!\n");
			   	}else{
				  	fd_fifo=open(NOME_FIFO_SERVIDOR, O_WRONLY);
					write(fd_fifo, &pedido, sizeof(pedido));
					close(fd_fifo);
					fd_fifo=open(nome_fifo_local, O_RDONLY);
					aux=read(fd_fifo, &resp_login, sizeof(resp_login));
					if(aux==sizeof(resp_login)){
					    switch(resp_login){
					        case -1:
					            wprinterror(win_consola, "Não consegui abrir o ficheiro!\n");
					        break;
					        case 0:
					            wprinterror(win_consola, "Não encontrou o utilizador!\n");
					        break;
					        case 1:
					            wprintlog(win_consola, "Encontrou o utilizador e a senha esta correta!\n");
				   				strcpy(pedido.user.name, comando_local_separado[1]);
								pedido.user.logado = true;  
					        break;
					        case 2:
					            wprinterror(win_consola, "Encontrou o utilizador, mas a senha está incorreta!\n");
					        break;
					        case 3:
					            wprinterror(win_consola, "Este utilizador já se encontra logado!\n");
					        break;
					    }
					}
					close(fd_fifo);
				   	if(ha_jogo && !dentro_jogo && pedido.user.logado){
						wprintlog(win_consola, "Exite um jogo a decorrer!\n      Faça <join [eq] [jo]> para entrar.\n");
					}
			    }
			    wrefresh(win_consola);
			    }else if(!strcmp(comando_local_separado[0], "join")){
			    c_equipa = atoi(comando_local_separado[1]);
			    c_jogador = atoi(comando_local_separado[2]);
			    if(pedido.user.logado){
					if(!dentro_jogo){
						if(c_equipa >= 0 && c_equipa <= 1){
							if(c_jogador >= 0 && c_jogador <= 9 && c_jogador != 5){
								fd_fifo=open(NOME_FIFO_SERVIDOR, O_WRONLY);
								write(fd_fifo, &pedido, sizeof(pedido));
								close(fd_fifo);
							}else{
								wprinterror(win_consola, "Jogado invalido! (0-9\\5)\n");
							}
						}else{
							wprinterror(win_consola, "Equipa invalida! (0-1)\n");
						}
					}else{
				    	wprinterror(win_consola, "Já está inscrito!\n       Para mudar faça logout.\n");
					}
			   	}else{
			   		wattron(win_consola, A_BOLD | COLOR_PAIR(2));
			        wprintw(win_consola, " Faca Login!\n");
			        wattroff(win_consola, A_BOLD | COLOR_PAIR(2));
			   	}
			    wrefresh(win_consola);
			}else if(!strcmp(comando_local_separado[0], "logout")){
			   	if(pedido.user.logado){
			   		fd_fifo=open(NOME_FIFO_SERVIDOR, O_WRONLY);
					write(fd_fifo, &pedido, sizeof(pedido));
					close(fd_fifo);
					dentro_jogo = false;
					pedido.user.logado = false;
					strcpy(pedido.user.name, "");
			   		wprintlog(win_consola, "Fez o logout!\n");
			   	}else{
			   		wattron(win_consola, A_BOLD | COLOR_PAIR(2));
			        wprintw(win_consola, " Faca Login!\n");
			        wattroff(win_consola, A_BOLD | COLOR_PAIR(2));
			   	}
			   	wrefresh(win_consola);
			}else if(!strcmp(comando_local_separado[0], "exit")){
			  	fd_fifo=open(NOME_FIFO_SERVIDOR, O_WRONLY);
				write(fd_fifo, &pedido, sizeof(pedido));
				close(fd_fifo);
			}
			strcpy(pedido.comando, "\0");
	    }while(strcmp(comando_local_separado[0], "exit"));

		terminaCliente();
		return EXIT_SUCCESS;
  	}
}
void trata_sinais(int sinal, siginfo_t *valor, void *n) {
	union sigval valor_sinal_trata_sinal;
  	switch (sinal) {
    	case SIGINT:
	    	if(ha_jogo && dentro_jogo && pedido.user.logado){
	    		wprinterror(win_consola, "Antes de sair com CTRL + C,\n       carregue me 'q' para desistir do jogo atual.\n");
	    	}else{
	    		if(valor->si_int != 3){
		    		valor_sinal_trata_sinal.sival_int = 2;
		    	    sigqueue(pid_servidor, SIGUSR2, valor_sinal_trata_sinal);
		    	}
		    	terminaCliente();
				exit(EXIT_SUCCESS);
	    	}
    	break;
    	case SIGUSR1:
    		pid_servidor = valor->si_pid;
    		ha_jogo = valor->si_int;
    	break;
    	case SIGUSR2:
      		if(valor->si_int == 0){
      			ha_jogo = false;
      			if(pedido.user.logado){
      				pthread_join(t_campo, NULL);
					unlink(fifo_jogador_sv);
      				wprintlog(win_consola, "Acabou jogo!\n");
      			}
      		}
      		if(valor->si_int == 1){
      			ha_jogo = true;
      			if(pedido.user.logado){
      				wprintlog(win_consola, "Começou jogo!\n      Faça <join [eq] [jo]> para entrar.\n");
      			}
      			if(dentro_jogo){
      				pthread_create(&t_campo, NULL, &modoJogo, NULL);
      			}
      		}
      		if(valor->si_int == 2){
      			dentro_jogo = false;
				pedido.user.logado = false;
				strcpy(pedido.user.name, "");
      			pthread_join(t_campo, NULL);
				unlink(fifo_jogador_sv);
      			wprintlog(win_consola, "Desistiu do jogo!\n");
      		}
      		wrefresh(win_consola);
    	break;
    	case SIGPIPE:
    		resultado_esquerda = valor->si_int;
    	break;
    	case SIGURG:
    		resultado_direita = valor->si_int;
    	break;
    	case SIGHUP:
	    	switch (valor->si_int){
	    		case 1:			
	    	    	pedido.user.logado = false;
					dentro_jogo = false;
					strcpy(pedido.user.name, "");
	      			pthread_join(t_campo, NULL);
					unlink(fifo_jogador_sv);
	      			wprinterror(win_consola, "Levou um vermelho, e foi expulso do servidor!\n");
	      		break;
	      		case 2:
					dentro_jogo = true;
					if(ha_jogo){
						pthread_create(&t_campo, NULL, &modoJogo, NULL);
					}
	      		break;
	      		case 3:
					wprinterror(win_consola, "Jogador ocupado! Repita o comando c/ 1 jogado diferete.\n");
					dentro_jogo = false;
	      		break;
	    	}
	  		wrefresh(win_consola); 
      	break;
  	}
}
void terminaCliente(){
	ha_jogo = false;
	pthread_join(t_campo, NULL);
	delwin(win_consola);
	endwin();
	unlink(nome_fifo_local);
}
void separaComandos(char str[]){
   char *token;
   char temp[80];
   const char s[] = " ";
   int i=0;
   strcpy(temp, str);
   token = strtok(temp, s);
   for(i = 0; i < 3; i++){
      if(token == NULL){
		strcpy(comando_local_separado[i], "");
      }else{
		strcpy(comando_local_separado[i], token);
      }
      token = strtok(NULL, s);
   }
}
void wprintlog(WINDOW *win, char str[]){
  wattron(win, A_BOLD | COLOR_PAIR(3));
    wprintw(win, "[LOG] ");
  wattroff(win, A_BOLD | COLOR_PAIR(3));
  wattron(win, A_BOLD);
    wprintw(win, "%s", str);
  wattroff(win, A_BOLD);
}
void wprinterror(WINDOW *win, char str[]){
  wattron(win, A_BOLD | COLOR_PAIR(2));
    wprintw(win, "[ERRO] ");
  wattroff(win, A_BOLD | COLOR_PAIR(2));
  wattron(win, A_BOLD);
    wprintw(win, "%s", str);
  wattroff(win, A_BOLD);
}
void desenhaCampo(WINDOW *win) {
    int x, y, i;
    getmaxyx(win, y, x); 

    for(i = 0; i < x; i++){
        if(i <= (x*0.15) || i >= (x-(x*0.15))){
            mvwaddch(win, (y*0.23), i, '-');
            mvwaddch(win, (y-(y*0.23)), i, '-');
        }
        mvwaddch(win, 0, i, '-');
        mvwaddch(win, y-1, i, '-');
    }
    for(i = 0; i<y; i++){
        if(i >= ((y*0.23)-1) && i <= (y-(y*0.23))){
            mvwaddch(win, i, (x*0.15), '|');
            mvwaddch(win, i, (x-(x*0.15)), '|');
        }
        mvwaddch(win, i, (x*0.5), '|');
        if(i >= (y*0.44)-1 && i <= (y-(y*0.44))){
            mvwaddch(win, i, 0, '#');
            mvwaddch(win, i, x-1, '#');
        }else{
            mvwaddch(win, i, 0, '|');
            mvwaddch(win, i, x-1, '|');
        }
    }
    mvwaddch(win, 0, 0, '+');
    mvwaddch(win, y-1, 0, '+');
    mvwaddch(win, 0, x-1, '+');
    mvwaddch(win, y-1, x-1, '+');
    mvwaddch(win, (y*0.23), (x*0.15), '+');
    mvwaddch(win, (y*0.23), x-(x*0.15), '+');
    mvwaddch(win, y-(y*0.23), (x*0.15), '+');
    mvwaddch(win, y-(y*0.23), x-(x*0.15), '+');
    mvwaddch(win, 0, (x*0.5), '+');
    mvwaddch(win, y-1, (x*0.5), '+');
    mvwaddch(win, (y*0.5), (x*0.5), '+');
}
void *atualizaInfo(){
	resultado_esquerda=0;
	resultado_direita=0;
	int ecra_y, ecra_x, posicao, fd_info;
	getmaxyx(stdscr, ecra_y, ecra_x);
	
	posicao = ecra_x / 2;
	wclear(win_info);
  	wattron(win_info, A_BOLD);
	wborder(win_info, '|', '|', '-', '-', '+', '+', '+', '+'); 
  	do{
		mvwaddch(win_info, 1, posicao-1, '|');
		mvwprintw(win_info, 1, (posicao/2)-3, "ESQ: %d", resultado_esquerda);
		mvwprintw(win_info, 1, ecra_x-((posicao/2)+3), "DIR: %d", resultado_direita);

  		wrefresh(win_info);
		usleep(250000);
  	}while(ha_jogo && dentro_jogo && pedido.user.logado);
  	wattroff(win_info, A_BOLD);
  	pthread_exit(0);
}
void *modoJogo(){
  	int aux, ecra_y, ecra_x, fd_modoJogo, i, posicao;
  	pthread_t t_info;

	getmaxyx(stdscr, ecra_y, ecra_x);

	WINDOW *win_campo = newwin(TAM_Y, TAM_X, 5, (ecra_x-TAM_X)/2);
	win_info = newwin(3, ecra_x, 1, 0);
  	wbkgd(win_info, COLOR_PAIR(8));

  	wbkgd(win_campo, COLOR_PAIR(4));

    curs_set(FALSE);

    noecho();

	wclear(win_consola);
	wrefresh(win_consola);
	wresize(win_consola, (ecra_y-TAM_Y-6), ecra_x);
	mvwin(win_consola, TAM_Y+6, 0);

	pthread_create(&t_info, NULL, &atualizaInfo, NULL);

	wprintlog(win_consola, "Começou o jogo!\n");
  	wrefresh(win_consola);

	wrefresh(win_info);
  	do{
  		fd_modoJogo = open(nome_fifo_local, O_RDONLY);
	    if(aux == sizeof(jogadores)){
			wclear(win_campo);
		    desenhaCampo(win_campo);
		    for(i = 0; i < TAM_JOGADORES; i++){
		    	if(jogadores[i].bola){
		    		if(jogadores[i].pid == pedido.user.pid){
		    			mvwaddch(win_campo, jogadores[i].ponto.y, jogadores[i].ponto.x, jogadores[i].idNum+'0' | COLOR_PAIR(7) | A_BOLD | A_UNDERLINE);
		    		}else{
		    			mvwaddch(win_campo, jogadores[i].ponto.y, jogadores[i].ponto.x, jogadores[i].idNum+'0' | COLOR_PAIR(7) | A_BOLD);
		    		}
		    	}else{
			    	if(jogadores[i].equipa == 0){
			    		if(jogadores[i].bola){
			    			mvwaddch(win_campo, jogadores[i].ponto.y, jogadores[i].ponto.x, jogadores[i].idNum+'0' | COLOR_PAIR(5) | A_BOLD);
			    		}else{
				    		if(jogadores[i].pid == pedido.user.pid){
					      		mvwaddch(win_campo, jogadores[i].ponto.y, jogadores[i].ponto.x, jogadores[i].idNum+'0' | COLOR_PAIR(5) | A_BOLD | A_UNDERLINE);
					      	}else{
					      		mvwaddch(win_campo, jogadores[i].ponto.y, jogadores[i].ponto.x, jogadores[i].idNum+'0' | COLOR_PAIR(5) | A_BOLD);
					      	}
			    		}
				      }else{
				      	if(jogadores[i].bola){

				      	}else{
					      	if(jogadores[i].pid == pedido.user.pid){
					      		mvwaddch(win_campo, jogadores[i].ponto.y, jogadores[i].ponto.x, jogadores[i].idNum+'0' | COLOR_PAIR(6) | A_BOLD | A_UNDERLINE);
					      	}else{
					      		mvwaddch(win_campo, jogadores[i].ponto.y, jogadores[i].ponto.x, jogadores[i].idNum+'0' | COLOR_PAIR(6) | A_BOLD);
					      	}
				      	}
				    }
		    	}
		    }
		    wrefresh(win_campo);
	    }
	    aux = read(fd_modoJogo, &jogadores, sizeof(jogadores));
	    close(fd_modoJogo);
  	}while(ha_jogo && dentro_jogo && pedido.user.logado);

    pthread_join(t_info, NULL);
  	delwin(win_campo);
  	delwin(win_info);

  	wclear(win_consola);
	wresize(win_consola, ecra_y, ecra_x);
	mvwin(win_consola, 0, 0);

    echo();

    curs_set(TRUE);

    wprintlog(win_consola, "Vou terminar a thread\n");

  	wrefresh(win_consola);

  	pthread_exit(0);
}