#include "util.h"

USER user[TAM_JOGADORES] = {"", false, 0}; //Guarda todos os PID's dos clientes que se ligam.
char comando_local_separado[3][25], comando_cliente_separado[3][25]; //Guarda o comando e o(s) seu(s) argumento(s)
char nome_ficheiro_userpass[55]; //Guarda o nome do ficheiro a utilizar
bool ha_jogo; //Flags
pthread_t thread_respondeFifos, thread_validaJogo;
pthread_mutex_t trinco;
PEDIDO pedido;
JOGADOR jogador[TAM_JOGADORES];
int resultado_esquerda = 0, resultado_direita = 0;
time_t t;

WINDOW *win_consola;

/* Prototipos das funcoes */
/* Funcao para validar o login, devolvendo diferentes resultados consuante o sucedido */
int validarLogin(char nomeFicheiro[], char utilizadorInput[], char passInput[]);

/* Funcao que trata os sinais */
void trata_sinais(int sinal, siginfo_t *valor, void *n);

/* Funcao para separar os comandos */
void separaComandos(char str[], int flag);

/* Funcao que se dedica a enviar o estado atual do jogo a todos os clientes */
void *controlaJogo();

/* Funcao que é responsavel pelo movimento dos jogadores controlados pelo computador */
void *move_jogador(void *dadosJogador);

/* Funcao q se dedica a resposta dos pedidos dos clientes */
void *respondeFifos();

/* Impressao formatada de um log */
void wprintlog(WINDOW *win, char str[]);

/* Impressao formatada de um erro */
void wprinterror(WINDOW *win, char str[]);

/* Encontra um pid atraves do nome */
int encontraPidUser(char name[]);

/* Encontra a equipa e o jogador de um certo cliente */
int encontraEqJoPID(int pid);

/* Conjuto de codigos para fechar o servidor */
void terminarServidor();

/* Limpa os dados de certo jogador */
bool resetJogadorPID(int pid);

/* Limpa todos os dados dos jogadores */
void resetJogadores();

/* Faz reset a posicao de todos os jogadores */
void resetPosicaoInicial();

/* Imprime o resultado formatado */
void printResult();

/* Termina jogo a todos os clientes */
void terminaJogoClientes();

/* Manda um sinal para terminar o jogo ao cliente indicado */
void terminarJogoaUmCliente(int pid);

/* Verifica se o jogador pode fazer certo movimento */
bool validaMovimento(int y, int x);

/* Passa o controlo da bola para o jogador indicado */
bool passaBola(int eq, int jog);

/* Verifica que existe um jogador com a bola no perimetro */
int verificaBolaPerimetro(int y, int x, int equipa);

/* Escolhe um jogador random (caso exista) numa posicao random */
int jogadorRandomNumEspaco(int min_y, int max_y, int min_x, int max_x);

/* Atualiza o score de todos os clientes atraves de sinais */
void atualizarScoreClientes();

/* Muda o jogador que o humano controla */
bool mudaJogador(int eq, int jog, int pid);

/* verifica a equipa de um jogador numa posicao */
int verificaEquipaPosicao(int y, int x);

/* Retorna a equipa q tem a bola */
int equipaBola();


int main(int argc, char *argv[]){
  int ecra_y, ecra_x, i, j;
  char comando[80], temp[80];
  bool flag_aux;
  FILE *ficheiroAbrir;
  srand((unsigned) time(&t));
  union sigval valor_sinal_main;

  /*
    Inicializa o tratamento de sinais
  */
  struct sigaction sinal;
  sigemptyset(&sinal.sa_mask);
  sinal.sa_sigaction = trata_sinais;
  sinal.sa_flags = SA_SIGINFO; /* Important. */

  sigaction(SIGALRM, &sinal, NULL);
  sigaction(SIGUSR1, &sinal, NULL);
  sigaction(SIGUSR2, &sinal, NULL);
  sigaction(SIGHUP, &sinal, NULL);
  sigaction(SIGINT, &sinal, NULL);

  /*
    Inicializa o ambiente ncurses
  */
	initscr();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLUE);
  init_pair(2, COLOR_RED, COLOR_BLUE);
  init_pair(3, COLOR_GREEN, COLOR_BLUE);
	clear();
	cbreak();

  /*
    Configuração da janela de trabalho.
  */
	getmaxyx(stdscr, ecra_y, ecra_x);
	win_consola = newwin(ecra_y, ecra_x, 0, 0);
	scrollok(win_consola, true);
  	wbkgd(win_consola, COLOR_PAIR(1));
  	resetJogadores();





	wprintw(win_consola, "%s\n\n", getenv("MENSAGEM"));

	/*//fprintf("%s\n",getenv("MENSAGEM"));
    wprintlog(win_consola,"%s", getenv("MENSAGEM"));*/
  	//printf("%s\n", getenv("MENSAGEM"));





  if(access(NOME_FIFO_SERVIDOR, F_OK) == 0){
    printf("[ERRO]  Ja existe um servidor em execucao!\n");
    delwin(win_consola);
    endwin();
    return EXIT_SUCCESS;
  }else{

    /* 
      Verifica que ficheiro deve utilizar para as palavras passes e utilizadores 
    */
    if(argc == 1){
        strcpy(nome_ficheiro_userpass, "passes.pwd");
    }else{
      if(argc == 2){
        strcpy(nome_ficheiro_userpass, argv[1]);
      }else{
        printf("[ERRO] Demasiados argumentos!\n");
        delwin(win_consola);
        endwin();
        return EXIT_SUCCESS;
      }
    }

    /*
      Configuração de todo o que é necessesário para correr o programa.
    */
    mkfifo(NOME_FIFO_SERVIDOR, 0600);
    ha_jogo = false;
    pthread_mutex_init(&trinco, NULL);
    pthread_create(&thread_respondeFifos, NULL, &respondeFifos, NULL);

    do{
      wscanw(win_consola, "%79[^\n]", comando);
      separaComandos(comando, 0);      
      if(!strcmp(comando_local_separado[0], "start")){
        resultado_direita = 0;
        resultado_esquerda = 0;
        atualizarScoreClientes();
        valor_sinal_main.sival_int = 1;
        ha_jogo = true;
        for(i = 0; i < TAM_JOGADORES; i++){
          if(user[i].pid != 0){
            if(sigqueue(user[i].pid, SIGUSR2, valor_sinal_main) != 0){
              wprinterror(win_consola, "Sinal não enviado.\n");
            }
          }
        }
        pthread_create(&thread_validaJogo, NULL, &controlaJogo, NULL);
        alarm(atoi(comando_local_separado[1]));
        sprintf(temp, "Começou jogo! Duraçao: %s segundos.\n", comando_local_separado[1]);
        wprintlog(win_consola, temp);
        wrefresh(win_consola);
      }else if(!strcmp(comando_local_separado[0], "stop")){
        alarm(0);
        valor_sinal_main.sival_int = 0;
        ha_jogo = false;
        for(i = 0; i < TAM_JOGADORES; i++){
          if(user[i].pid != 0){
            sigqueue(user[i].pid, SIGUSR2, valor_sinal_main);
          }
        }
        terminaJogoClientes();
        wprintlog(win_consola, "Jogo parado!\n");
        printResult();
        wrefresh(win_consola);
      }else if(!strcmp(comando_local_separado[0], "user")){
        if(validarLogin(nome_ficheiro_userpass, comando_local_separado[1], "")==2){
          sprintf(temp, "User \'%s\'' já existe! Escolha outro nome!\n", comando_local_separado[1]);
          wprinterror(win_consola, temp);
        }else{
          ficheiroAbrir = fopen(nome_ficheiro_userpass, "a");
          if(ficheiroAbrir != NULL){
            fprintf(ficheiroAbrir, "%s:%s\n", comando_local_separado[1], comando_local_separado[2]);
            wprintlog(win_consola,"User criado com sucesso!\n");
          }else{
            wprinterror(win_consola,"Não consegui abrir o ficheiro...\n");
          }
          fclose(ficheiroAbrir);
        }
        wrefresh(win_consola);
      }else if(!strcmp(comando_local_separado[0], "matriz")){
        for(j = 0; j < TAM_JOGADORES; j++){
            wattron(win_consola, A_BOLD);
            wprintw(win_consola, "[%d%d] [%d]\n", jogador[j].equipa, jogador[j].idNum, jogador[j].pid);
            wattroff(win_consola, A_BOLD);
        }
      }else if(!strcmp(comando_local_separado[0], "users")){
        flag_aux = false;
        wattron(win_consola, A_BOLD | COLOR_PAIR(3));
        wprintw(win_consola, "[users]\n");
        wattroff(win_consola, A_BOLD | COLOR_PAIR(3));
        for(i = 0; i < TAM_JOGADORES; i++){
          if(user[i].pid != 0){
            flag_aux = true;
            wattron(win_consola, A_BOLD);
            wprintw(win_consola, "%d) %d ", i, user[i].pid);
            wattroff(win_consola, A_BOLD);
            if(user[i].logado){
              wattron(win_consola, A_BOLD | COLOR_PAIR(3));
              wprintw(win_consola, "[%s] ", user[i].name);
              wattroff(win_consola, A_BOLD | COLOR_PAIR(3));
              j = encontraEqJoPID(user[i].pid);
              if(j != -1){
                if(j <=9){
                  wprintw(win_consola, "[0%d] ", j);
                }else{
                  wprintw(win_consola, "[%d] ", j);
                }
              }
              wprintw(win_consola, "\n");
            }else{
              wattron(win_consola, A_BOLD | COLOR_PAIR(2));
              wprintw(win_consola, "[nao logado]\n");
              wattroff(win_consola, A_BOLD | COLOR_PAIR(2)); 
            }
          }
        }          
        if(!flag_aux){
          wattron(win_consola, A_BOLD | COLOR_PAIR(2));
          wprintw(win_consola, " [nao há clientes]\n");
          wattroff(win_consola, A_BOLD | COLOR_PAIR(2)); 
        }
        wrefresh(win_consola);
      }else if(!strcmp(comando_local_separado[0], "red")){
        valor_sinal_main.sival_int = 1;
        for(i = 0; i < TAM_JOGADORES; i++){
          if(user[i].pid == encontraPidUser(comando_local_separado[1])){
            if(ha_jogo && encontraEqJoPID(user[i].pid) != -1){
              sigqueue(user[i].pid, SIGHUP, valor_sinal_main);
              user[i].logado = false;
              strcpy(user[i].name,"");
              terminarJogoaUmCliente(user[i].pid);
              resetJogadorPID(user[i].pid);
              break;
            }else{
              wprinterror(win_consola, "Não pode dar vermelhos a clientes que não estão a jogar!");
            }
          }
        }
      }else if(!strcmp(comando_local_separado[0], "result")){
        if(ha_jogo){
          printResult();
        }else{
          wprinterror(win_consola, "Não esta a decorrer nenhum jogo!\n");
        }
        wrefresh(win_consola);
      }
      if(ha_jogo && strcmp(comando_local_separado[0], "shutdown")==0){
        wprinterror(win_consola, "Está a decorrer um jogo!\n       Faça <stop> antes de sair.\n");
        strcpy(comando_local_separado[0], "\0");
        wrefresh(win_consola);
      }
      strcpy(comando, "\n");
    }while(strcmp(comando_local_separado[0], "shutdown"));

    terminarServidor();

    return EXIT_SUCCESS;
  }
}

void terminarServidor(){
  union sigval valor;
  int i;
  ha_jogo = false;
  for(i = 0; i < TAM_JOGADORES; i++){
    if(user[i].pid != 0){
      sigqueue(user[i].pid, SIGINT, valor);
    }
  }
  terminaJogoClientes();
  pthread_cancel(thread_respondeFifos);
  pthread_mutex_destroy(&trinco);
  delwin(win_consola);
  endwin();
  unlink(NOME_FIFO_SERVIDOR);
}

void trata_sinais(int sinal, siginfo_t *valor, void *n) {
  union sigval valor_sinal_trata_sinal;
  int i;
  switch (sinal) {
    case SIGALRM:
        valor_sinal_trata_sinal.sival_int = 0;
        ha_jogo = false;
        for(i = 0; i < TAM_JOGADORES; i++){
          if(user[i].logado){
            sigqueue(user[i].pid, SIGUSR2, valor_sinal_trata_sinal);
          }
        }
        terminaJogoClientes();
        wprintlog(win_consola, "Jogo terminado!\n");
        printResult();
    break;
    case SIGUSR1:
      terminarServidor();
      exit(EXIT_SUCCESS);
    break;
    case SIGUSR2:
      if(valor->si_int == 2){
        for(i=0; i < TAM_JOGADORES; i++){
          if(user[i].pid == valor->si_pid){
            resetJogadorPID(user[i].pid);
            user[i].logado = false;
            strcpy(user[i].name,"");
            user[i].pid = 0;
            break;
          }
        }
      }
    break;
    case SIGHUP:
      if(valor->si_int == 0){
        valor_sinal_trata_sinal.sival_int = 2;
        if(sigqueue(valor->si_pid, SIGUSR2, valor_sinal_trata_sinal)==0){
          for(i = 0; i < TAM_JOGADORES; i++){
            if(user[i].pid == valor->si_pid){
              user[i].logado = false;
              strcpy(user[i].name,"");
              terminarJogoaUmCliente(valor->si_pid);
              resetJogadorPID(valor->si_pid);
              wprintlog(win_consola, "Um jogador desistiu!\n");
              wrefresh(win_consola);
            }
          }
        }
      }
    break;
    case SIGINT:
      if(ha_jogo){
        wprinterror(win_consola, "Está haver um jogo neste momento!\n       Espero até que acabe ou faça <stop> para o terminar.\n");
        wrefresh(win_consola);
      }else{
        terminarServidor();
        exit(EXIT_SUCCESS);
      }
    break;
  }
}

bool validaMovimento(int y, int x){
  int i;
  if(y < 0 || y > TAM_Y-1 || x < 0 || x > TAM_X-1 ){
    return false;
  }else{
    for(i = 0; i < TAM_JOGADORES; i++){
      if(jogador[i].ponto.y == y && jogador[i].ponto.x == x){
        return false;
      }
    }
    return true;
  }
}

void *move_jogador(void *dadosJogador){
  int tecla, fd, aux, remate, roubabola, ha_jogador, bola_perdida;
  char fifoClienteJogador[25];
  JOGADOR *pJog;
  pJog = (JOGADOR *) dadosJogador;
int random;

//---------------jogador automatico 
  do{
    if(pJog->pid == 0){
      tecla = rand() % 5;
      pthread_mutex_lock(&trinco);
      switch(tecla){
        case 0:
        random = rand() % 100 + 1;
        if(equipaBola() == pJog->equipa && pJog->idNum >=6 && pJog->idNum <= 9 && random >=55 && random <= 100){
        	if(pJog->equipa == 1){
	          if(validaMovimento(pJog->ponto.y, pJog->ponto.x-1)){
	                if(pJog->equipa==1 && pJog->idNum == 0){
	                  if((pJog->ponto.x-1) >= (TAM_X-7)){
	                    pJog->ponto.x--;
	                  }
	                }else{
	                  pJog->ponto.x--;
	                }
	          }
        	}else{
	          if(validaMovimento(pJog->ponto.y, pJog->ponto.x+1)){
	            if(pJog->equipa==0 && pJog->idNum == 0){
	              if((pJog->ponto.x+1) < 7){
	                pJog->ponto.x++;
	              }
	            }else{
	              pJog->ponto.x++;
	            }
	          }
        	}
        }else{
          if(validaMovimento(pJog->ponto.y-1, pJog->ponto.x)){
            if(pJog->idNum==0){
              if((pJog->ponto.y-1) >= 5){
                pJog->ponto.y--;
              }
            }else{
              pJog->ponto.y--;
            }
          }
        }
          if(!pJog->bola){
            ha_jogador = verificaBolaPerimetro(pJog->ponto.y, pJog->ponto.x, pJog->equipa);
            if(ha_jogador != -1){
              roubabola = rand() % 100 + 1;
          	    if(roubabola >= 0 && roubabola <= 85){
            	    jogador[ha_jogador].bola = false;
                    pJog->bola = true;
                }
            }
        }



        break;
        case 1:
                if(equipaBola() == pJog->equipa && pJog->idNum >=6 && pJog->idNum <= 9 && random >=55 && random <= 100){
        	if(pJog->equipa == 1){
	          if(validaMovimento(pJog->ponto.y, pJog->ponto.x-1)){
	                if(pJog->equipa==1 && pJog->idNum == 0){
	                  if((pJog->ponto.x-1) >= (TAM_X-7)){
	                    pJog->ponto.x--;
	                  }
	                }else{
	                  pJog->ponto.x--;
	                }
	          }
        	}else{
	          if(validaMovimento(pJog->ponto.y, pJog->ponto.x+1)){
	            if(pJog->equipa==0 && pJog->idNum == 0){
	              if((pJog->ponto.x+1) < 7){
	                pJog->ponto.x++;
	              }
	            }else{
	              pJog->ponto.x++;
	            }
	          }
        	}
        }else{
          if(validaMovimento(pJog->ponto.y+1, pJog->ponto.x)){
            if(pJog->idNum==0){
              if((pJog->ponto.y+1) < (TAM_Y-5)){
                pJog->ponto.y++;
              }
            }else{
              pJog->ponto.y++;
            }
          }
      }
          if(!pJog->bola){
            ha_jogador = verificaBolaPerimetro(pJog->ponto.y, pJog->ponto.x, pJog->equipa);
            if(ha_jogador != -1){
              roubabola = rand() % 100 + 1;
          	    if(roubabola >= 0 && roubabola <= 85){
            	    jogador[ha_jogador].bola = false;
                    pJog->bola = true;
                }
            }
        }
        break;
        case 2:
                if(equipaBola() == pJog->equipa && pJog->idNum >=6 && pJog->idNum <= 9 && random >=55 && random <= 100){
        	if(pJog->equipa == 1){
	          if(validaMovimento(pJog->ponto.y, pJog->ponto.x-1)){
	                if(pJog->equipa==1 && pJog->idNum == 0){
	                  if((pJog->ponto.x-1) >= (TAM_X-7)){
	                    pJog->ponto.x--;
	                  }
	                }else{
	                  pJog->ponto.x--;
	                }
	          }
        	}else{
	          if(validaMovimento(pJog->ponto.y, pJog->ponto.x+1)){
	            if(pJog->equipa==0 && pJog->idNum == 0){
	              if((pJog->ponto.x+1) < 7){
	                pJog->ponto.x++;
	              }
	            }else{
	              pJog->ponto.x++;
	            }
	          }
        	}
        }else{
          if(validaMovimento(pJog->ponto.y, pJog->ponto.x-1)){
                if(pJog->equipa==1 && pJog->idNum == 0){
                  if((pJog->ponto.x-1) >= (TAM_X-7)){
                    pJog->ponto.x--;
                  }
                }else{
                  pJog->ponto.x--;
                }
          }
      }
          if(!pJog->bola){
            ha_jogador = verificaBolaPerimetro(pJog->ponto.y, pJog->ponto.x, pJog->equipa);
            if(ha_jogador != -1){
              roubabola = rand() % 100 + 1;
          	    if(roubabola >= 0 && roubabola <= 85){
            	    jogador[ha_jogador].bola = false;
                    pJog->bola = true;
                }
            }
        }
        break;
        case 3:
                if(equipaBola() == pJog->equipa && pJog->idNum >=6 && pJog->idNum <= 9 && random >=55 && random <= 100){
        	if(pJog->equipa == 1){
	          if(validaMovimento(pJog->ponto.y, pJog->ponto.x-1)){
	                if(pJog->equipa==1 && pJog->idNum == 0){
	                  if((pJog->ponto.x-1) >= (TAM_X-7)){
	                    pJog->ponto.x--;
	                  }
	                }else{
	                  pJog->ponto.x--;
	                }
	          }
        	}else{
	          if(validaMovimento(pJog->ponto.y, pJog->ponto.x+1)){
	            if(pJog->equipa==0 && pJog->idNum == 0){
	              if((pJog->ponto.x+1) < 7){
	                pJog->ponto.x++;
	              }
	            }else{
	              pJog->ponto.x++;
	            }
	          }
        	}
        }else{
          if(validaMovimento(pJog->ponto.y, pJog->ponto.x+1)){
            if(pJog->equipa==0 && pJog->idNum == 0){
              if((pJog->ponto.x+1) < 7){
                pJog->ponto.x++;
              }
            }else{
              pJog->ponto.x++;
            }
          }
      }
          if(!pJog->bola){
            ha_jogador = verificaBolaPerimetro(pJog->ponto.y, pJog->ponto.x, pJog->equipa);
            if(ha_jogador != -1){
              roubabola = rand() % 100 + 1;
          	    if(roubabola >= 0 && roubabola <= 85){
            	    jogador[ha_jogador].bola = false;
                    pJog->bola = true;
                }
            }
        }

        break;
        case 4:              
        	if(pJog->bola){
	        	random = rand() % 100 + 1;
	        	if(random >= 1 && random <= 30){
	                remate = rand() % 100 + 1;
	                if(pJog->idNum==0){
	                  if(remate>=0 && remate <=25){
	                    if(pJog->equipa == 0){
	                      resultado_esquerda++;
	                      atualizarScoreClientes();
	                      passaBola(1, 0);
	                    }else{
	                      resultado_direita++;
	                      atualizarScoreClientes();
	                      passaBola(0, 0);
	                    }
	                    printResult();
	                    wrefresh(win_consola);
	                  }else{
	                    if(pJog->equipa == 0){
	                      bola_perdida = jogadorRandomNumEspaco(0, 20, 25, 50);
	                      if(bola_perdida!=-1){
	                        jogador[bola_perdida].bola = true;
	                      }else{
	                        passaBola(1, 0);
	                      }
	                    }else{
	                      bola_perdida = jogadorRandomNumEspaco(0, 20, 0, 25);
	                      if(bola_perdida!=-1){
	                        jogador[bola_perdida].bola = true;
	                      }else{
	                        passaBola(0, 0);
	                      }
	                    }
	                  }
	                }
	                if(pJog->idNum>=1 && pJog->idNum<=4){
	                  if(remate>=0 && remate <=80){
	                    if(pJog->equipa == 0){
	                      resultado_esquerda++;
	                      atualizarScoreClientes();
	                      passaBola(1, 0);
	                    }else{
	                      resultado_direita++;
	                      atualizarScoreClientes();
	                      passaBola(0, 0);
	                    }
	                    printResult();
	                    wrefresh(win_consola);
	                  }else{
	                    if(pJog->equipa == 0){
	                      bola_perdida = jogadorRandomNumEspaco(0, 20, 25, 50);
	                      if(bola_perdida!=-1){
	                        jogador[bola_perdida].bola = true;
	                      }else{
	                        passaBola(1, 0);
	                      }
	                    }else{
	                      bola_perdida = jogadorRandomNumEspaco(0, 20, 0, 25);
	                      if(bola_perdida!=-1){
	                        jogador[bola_perdida].bola = true;
	                      }else{
	                        passaBola(0, 0);
	                      }
	                    }
	                  }
	                }
	                if(pJog->idNum>=6 && pJog->idNum<=9){
	                  if(remate>=0 && remate <=60){
	                    if(pJog->equipa == 0){
	                      resultado_esquerda++;
	                      atualizarScoreClientes();
	                      passaBola(1, 0);
	                    }else{
	                      resultado_direita++;
	                      atualizarScoreClientes();
	                      passaBola(0, 0);
	                    }
	                  }else{
	                    if(pJog->equipa == 0){
	                      bola_perdida = jogadorRandomNumEspaco(0, 20, 25, 50);
	                      if(bola_perdida!=-1){
	                        jogador[bola_perdida].bola = true;
	                      }else{
	                        passaBola(1, 0);
	                      }
	                      printResult();
	                      wrefresh(win_consola);
	                    }else{
	                      bola_perdida = jogadorRandomNumEspaco(0, 20, 0, 25);
	                      if(bola_perdida!=-1){
	                        jogador[bola_perdida].bola = true;
	                      }else{
	                        passaBola(0, 0);
	                      }
	                    }
	                  }
	                }
	                pJog->bola = false;
	        	}
	        	if(random >= 31 && random <= 81){
	        		do{
	        			random = rand() % 10;
	        		}while(random == 5 || random == pJog->idNum);
	        		passaBola(pJog->equipa, random);
	                pJog->bola = false;
	        	}
            }
        break;
      }

      pthread_mutex_unlock(&trinco);

//--------------jogador com tecla humano 

    }else{


      sprintf(fifoClienteJogador, "sv%d", pJog->pid);
      fd = open(fifoClienteJogador, O_RDONLY);
      do{
        aux = read(fd, &tecla, sizeof(tecla));
        if(aux == sizeof(tecla)){
          pthread_mutex_lock(&trinco);
          switch(tecla){
            case KEY_UP:
              if(validaMovimento(pJog->ponto.y-1, pJog->ponto.x)){
                if(pJog->idNum==0){
                  if((pJog->ponto.y-1) >= 5){
                    pJog->ponto.y--;
                  }
                }else{
                  pJog->ponto.y--;
                }
              }
              if(!pJog->bola){
                ha_jogador = verificaBolaPerimetro(pJog->ponto.y, pJog->ponto.x, pJog->equipa);
                if(ha_jogador != -1){
                  roubabola = rand() % 100 + 1;
                  if(roubabola >= 0 && roubabola <= 85){
                    jogador[ha_jogador].bola = false;
                    pJog->bola = true;
                  }
                }
              }
            break;
            case KEY_DOWN:
              if(validaMovimento(pJog->ponto.y+1, pJog->ponto.x)){
                if(pJog->idNum==0){
                  if((pJog->ponto.y+1) < (TAM_Y-5)){
                    pJog->ponto.y++;
                  }
                }else{
                  pJog->ponto.y++;
                }
              }
              if(!pJog->bola){
                ha_jogador = verificaBolaPerimetro(pJog->ponto.y, pJog->ponto.x, pJog->equipa);
                if(ha_jogador != -1){
                  roubabola = rand() % 100 + 1;
                  if(roubabola >= 0 && roubabola <= 85){
                    jogador[ha_jogador].bola = false;
                    pJog->bola = true;
                  }
                }
              }
            break;
            case KEY_LEFT:
              if(validaMovimento(pJog->ponto.y, pJog->ponto.x-1)){
                if(pJog->equipa==1 && pJog->idNum == 0){
                  if((pJog->ponto.x-1) >= (TAM_X-7)){
                    pJog->ponto.x--;
                  }
                }else{
                  pJog->ponto.x--;
                }
              }
              if(!pJog->bola){
                ha_jogador = verificaBolaPerimetro(pJog->ponto.y, pJog->ponto.x, pJog->equipa);
                if(ha_jogador != -1){
                  roubabola = rand() % 100 + 1;
                  if(roubabola >= 0 && roubabola <= 85){
                    jogador[ha_jogador].bola = false;
                    pJog->bola = true;
                  }
                }
              }
            break;
            case KEY_RIGHT:
              if(validaMovimento(pJog->ponto.y, pJog->ponto.x+1)){
                if(pJog->equipa==0 && pJog->idNum == 0){
                  if((pJog->ponto.x+1) < 7){
                    pJog->ponto.x++;
                  }
                }else{
                  pJog->ponto.x++;
                }
              }
              if(!pJog->bola){
                ha_jogador = verificaBolaPerimetro(pJog->ponto.y, pJog->ponto.x, pJog->equipa);
                if(ha_jogador != -1){
                  roubabola = rand() % 100 + 1;
                  if(roubabola >= 0 && roubabola <= 85){
                    jogador[ha_jogador].bola = false;
                    pJog->bola = true;
                  }
                }
              }
            break;
            case '0':
              if(pJog->bola && pJog->idNum != 0){
                passaBola(pJog->equipa, 0);
                pJog->bola = false;
              }else{
                int p;
                if(mudaJogador(pJog->equipa, 0, pJog->pid)){
                    pJog->pid = 0;
                }
              }
            break;
            case '1':
              if(pJog->bola && pJog->idNum != 1){
                passaBola(pJog->equipa, 1);
                pJog->bola = false;
              }else{
                int p;
                if(mudaJogador(pJog->equipa, 1, pJog->pid)){
                    pJog->pid = 0;
                }
              }
            break;
            case '2':
              if(pJog->bola && pJog->idNum != 2){
                passaBola(pJog->equipa, 2);
                pJog->bola = false;
              }else{
                if(mudaJogador(pJog->equipa, 2, pJog->pid)){
                    pJog->pid = 0;
                }
              }
            break;
            case '3':
              if(pJog->bola && pJog->idNum != 3){
                passaBola(pJog->equipa, 3);
                pJog->bola = false;
              }else{
                if(mudaJogador(pJog->equipa, 3, pJog->pid)){
                    pJog->pid = 0;
                }
              }
            break;
            case '4':
              if(pJog->bola && pJog->idNum != 4){
                passaBola(pJog->equipa, 4);
                pJog->bola = false;
              }else{
                if(mudaJogador(pJog->equipa, 4, pJog->pid)){
                    pJog->pid = 0;
                }
              }
            break;
            case '5':
              if(pJog->bola){
                remate = rand() % 100 + 1;
                if(pJog->idNum==0){
                  if(remate>=0 && remate <=25){
                    if(pJog->equipa == 0){
                      resultado_esquerda++;
                      atualizarScoreClientes();
                      passaBola(1, 0);
                    }else{
                      resultado_direita++;
                      atualizarScoreClientes();
                      passaBola(0, 0);
                    }
                    printResult();
                    wrefresh(win_consola);
                  }else{
                    if(pJog->equipa == 0){
                      bola_perdida = jogadorRandomNumEspaco(0, 20, 25, 50);
                      if(bola_perdida!=-1){
                        jogador[bola_perdida].bola = true;
                      }else{
                        passaBola(1, 0);
                      }
                    }else{
                      bola_perdida = jogadorRandomNumEspaco(0, 20, 0, 25);
                      if(bola_perdida!=-1){
                        jogador[bola_perdida].bola = true;
                      }else{
                        passaBola(0, 0);
                      }
                    }
                  }
                }
                if(pJog->idNum>=1 && pJog->idNum<=4){
                  if(remate>=0 && remate <=80){
                    if(pJog->equipa == 0){
                      resultado_esquerda++;
                      atualizarScoreClientes();
                      passaBola(1, 0);
                    }else{
                      resultado_direita++;
                      atualizarScoreClientes();
                      passaBola(0, 0);
                    }
                    printResult();
                    wrefresh(win_consola);
                  }else{
                    if(pJog->equipa == 0){
                      bola_perdida = jogadorRandomNumEspaco(0, 20, 25, 50);
                      if(bola_perdida!=-1){
                        jogador[bola_perdida].bola = true;
                      }else{
                        passaBola(1, 0);
                      }
                    }else{
                      bola_perdida = jogadorRandomNumEspaco(0, 20, 0, 25);
                      if(bola_perdida!=-1){
                        jogador[bola_perdida].bola = true;
                      }else{
                        passaBola(0, 0);
                      }
                    }
                  }
                }
                if(pJog->idNum>=6 && pJog->idNum<=9){
                  if(remate>=0 && remate <=60){
                    if(pJog->equipa == 0){
                      resultado_esquerda++;
                      atualizarScoreClientes();
                      passaBola(1, 0);
                    }else{
                      resultado_direita++;
                      atualizarScoreClientes();
                      passaBola(0, 0);
                    }
                  }else{
                    if(pJog->equipa == 0){
                      bola_perdida = jogadorRandomNumEspaco(0, 20, 25, 50);
                      if(bola_perdida!=-1){
                        jogador[bola_perdida].bola = true;
                      }else{
                        passaBola(1, 0);
                      }
                      printResult();
                      wrefresh(win_consola);
                    }else{
                      bola_perdida = jogadorRandomNumEspaco(0, 20, 0, 25);
                      if(bola_perdida!=-1){
                        jogador[bola_perdida].bola = true;
                      }else{
                        passaBola(0, 0);
                      }
                    }
                  }
                }
                pJog->bola = false;
              }
            break;
            case '6':
              if(pJog->bola && pJog->idNum != 6){
                passaBola(pJog->equipa, 6);
                pJog->bola = false;
              }else{
                if(mudaJogador(pJog->equipa, 6, pJog->pid)){
                    pJog->pid = 0;
                }
              }
            break;
            case '7':
              if(pJog->bola && pJog->idNum != 7){
                passaBola(pJog->equipa, 7);
                pJog->bola = false;
              }else{
                if(mudaJogador(pJog->equipa, 7, pJog->pid)){
                    pJog->pid = 0;
                }
              }
            break;
            case '8':
              if(pJog->bola && pJog->idNum != 8){
                passaBola(pJog->equipa, 8);
                pJog->bola = false;
              }else{
                if(mudaJogador(pJog->equipa, 8, pJog->pid)){
                    pJog->pid = 0;
                }
              }
            break;
            case '9':
              if(pJog->bola && pJog->idNum != 9){
                passaBola(pJog->equipa, 9);
                pJog->bola = false;
              }else{
                if(mudaJogador(pJog->equipa, 9, pJog->pid)){
                    pJog->pid = 0;
                }
              }
            break;
          }
          pthread_mutex_unlock(&trinco);
        }
      }while(pJog->pid != 0);
      close(fd);
    }
    if(pJog->idNum == 1 || pJog->idNum == 2 || pJog->idNum == 3 || pJog->idNum == 4){
      usleep(400000);
    }else{
      usleep(300000);
    }
  }while(ha_jogo);
  pthread_exit(0);
}

bool passaBola(int eq, int jog){    
  int i;
  for(i = 0; i < TAM_JOGADORES; i++){
    if(jogador[i].equipa == eq && jogador[i].idNum == jog){
      jogador[i].bola = true;
      return true;
    }
  }
  return false;
}

int verificaBolaPerimetro(int y, int x, int equipa){
  int i;
  for(i = 0; i < TAM_JOGADORES; i++){
    if(jogador[i].ponto.y==(y-1) && jogador[i].ponto.x==(x-1)){
      if(jogador[i].bola && equipa != jogador[i].equipa){
        return i;
      }
    }else{
      if(jogador[i].ponto.y==(y-1) && jogador[i].ponto.x==(x)){
        if(jogador[i].bola && equipa != jogador[i].equipa){
          return i;
        }
      }else{
        if(jogador[i].ponto.y==(y-1) && jogador[i].ponto.x==(x+1)){
          if(jogador[i].bola && equipa != jogador[i].equipa){
            return i;
          }
        }else{
          if(jogador[i].ponto.y==(y) && jogador[i].ponto.x==(x-1)){
            if(jogador[i].bola && equipa != jogador[i].equipa){
              return i;
            }
          }else{
            if(jogador[i].ponto.y==(y) && jogador[i].ponto.x==(x+1)){
              if(jogador[i].bola && equipa != jogador[i].equipa){
                return i;
              }
            }else{
              if(jogador[i].ponto.y==(y+1) && jogador[i].ponto.x==(x-1)){
                if(jogador[i].ponto.y==(y+1) && jogador[i].ponto.x==(x)){
                  if(jogador[i].bola && equipa != jogador[i].equipa){
                    return i;
                  }
                }else{
                  if(jogador[i].ponto.y==(y+1) && jogador[i].ponto.x==(x+1)){
                    if(jogador[i].bola && equipa != jogador[i].equipa){
                      return i;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return -1;
}

int jogadorRandomNumEspaco(int min_y, int max_y, int min_x, int max_x){
  int i, j=0;
  int jogIndex[TAM_JOGADORES] = {-1};
  for(i = 0; i < TAM_JOGADORES; i++){
    if(jogador[i].ponto.y >= min_y && jogador[i].ponto.y <= max_y && jogador[i].ponto.x >= min_x && jogador[i].ponto.x <= max_x){
      jogIndex[j]=i;
      j++;
    }
  }
  i = rand() % j;
  return jogIndex[i];
}

void *controlaJogo(void *dadosJogador){
  int i, fd_validajogo, rdn, roubabola, ha_jogador;
  JOGADOR *pJog;
  pJog = (JOGADOR *) dadosJogador;
  char fifoDoJogoCliente[25];
  pthread_t thread_jogadores[TAM_JOGADORES];

  jogador[rand() % (TAM_JOGADORES+1)].bola = true;  

  for(i = 0; i < TAM_JOGADORES; i++){
    pthread_create(&thread_jogadores[i], NULL, &move_jogador, (void *) &jogador[i]);
  }

  do{
              
    for(i = 0; i < TAM_JOGADORES; i++){
      if(jogador[i].pid != 0){
        sprintf(fifoDoJogoCliente, "fifo%d", jogador[i].pid);
        fd_validajogo = open(fifoDoJogoCliente, O_WRONLY);
        write(fd_validajogo, &jogador, sizeof(jogador));
        close(fd_validajogo);
      }        
    }

    usleep(40000); //25fps  
  }while(ha_jogo);

  for(i = 0; i < TAM_JOGADORES; i++){
    pthread_cancel(thread_jogadores[i]);
  }

  resetPosicaoInicial();
  pthread_exit(0);
}

void *respondeFifos(){
  int fd_fifo, fd_resposta, aux, i, j, resp_login, c_equipa, c_jogador;
  union sigval valor_sinal;
  char nome_fifo_cliente[25], temp[255];
	while(true){ 
    fd_fifo = open(NOME_FIFO_SERVIDOR, O_RDONLY);
    aux = read(fd_fifo, &pedido, sizeof(pedido));
    if(aux == sizeof(pedido)){
      sprintf(temp, "[%d] %s\n", pedido.user.pid, pedido.comando);
      wprintlog(win_consola, temp);
      wrefresh(win_consola);
      separaComandos(pedido.comando, 1);
      sprintf(nome_fifo_cliente, "fifo%d", pedido.user.pid); //Escreve na string 'nomeFifoCLiente' = fifo[pid]
      if(!strcmp(comando_cliente_separado[0], "ola")){
        for(i = 0; i < TAM_JOGADORES; i++){
          if(user[i].pid == 0){
            user[i].pid = pedido.user.pid;
            valor_sinal.sival_int = ha_jogo;
            sigqueue(user[i].pid, SIGUSR1, valor_sinal);
            break;
          }
        }
      }
      if(!strcmp(comando_cliente_separado[0], "login")){
        if(encontraPidUser(comando_cliente_separado[1])==0){
          resp_login = validarLogin(nome_ficheiro_userpass, comando_cliente_separado[1], comando_cliente_separado[2]);
        }else{
          resp_login = 3;
        }
        fd_resposta = open(nome_fifo_cliente, O_WRONLY);
        write(fd_resposta, &resp_login, sizeof(resp_login));
        close(fd_resposta);
        if(resp_login == 1){
          for(i = 0; i < TAM_JOGADORES; i++){
            if(user[i].pid == pedido.user.pid){
              user[i].logado = true;
              strcpy(user[i].name, comando_cliente_separado[1]);
              break;
            }
          }
        }
      }
      if(!strcmp(comando_cliente_separado[0], "join")){
        c_equipa = atoi(comando_cliente_separado[1]);
        c_jogador = atoi(comando_cliente_separado[2]);
        for(i = 0; i < TAM_JOGADORES; i++){
          if(jogador[i].equipa == c_equipa && jogador[i].idNum == c_jogador){
            if(jogador[i].pid == 0){
              jogador[i].pid = pedido.user.pid;
              valor_sinal.sival_int = 2;
            }else{
              valor_sinal.sival_int = 3;
            }
          }
        }
        sigqueue(pedido.user.pid, SIGHUP, valor_sinal);
      }
      if(!strcmp(comando_cliente_separado[0], "logout")){
        for(i = 0; i < TAM_JOGADORES; i++){
          if(user[i].pid == pedido.user.pid){
            user[i].logado = false;
            strcpy(user[i].name,"");
            resetJogadorPID(user[i].pid);
          }
        }
      }
      if(!strcmp(comando_cliente_separado[0], "exit")){
        for(i = 0; i < TAM_JOGADORES; i++){
          if(user[i].pid == pedido.user.pid){
            resetJogadorPID(user[i].pid);
            user[i].logado = false;
            user[i].pid = 0;
            strcpy(user[i].name,"");
          }
        }
      }
    }
    close(fd_fifo);
	}
  pthread_exit(0);
}

int encontraPidUser(char name[]){
  int i;
  if(strcmp(name, "")){
    for(i = 0; i < TAM_JOGADORES; i++){
      if(!strcmp(user[i].name, name)){
        return user[i].pid;
      }
    }
  }
  return 0;
}

int encontraEqJoPID(int pid){
  int i;
  char temp[9];
  for(i = 0; i < TAM_JOGADORES; i++){
    if(jogador[i].pid==pid){
      sprintf(temp, "%d%d", jogador[i].equipa, jogador[i].idNum);
      return (atoi(temp));
    }
  }
  return -1;
}

void separaComandos(char str[], int flag){
   char *token;
   const char s[] = " ";
   int i=0;
   token = strtok(str, s);
   for(i = 0; i < 3; i++){
      if(token == NULL){
        if(flag == 0){
          strcpy(comando_local_separado[i], "");
        }else if(flag == 1){
          strcpy(comando_cliente_separado[i], "");
        }
      }else{
        if(flag == 0){
          strcpy(comando_local_separado[i], token);
        }else if(flag == 1){
          strcpy(comando_cliente_separado[i], token);
        }
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

int validarLogin(char nomeFicheiro[], char utilizadorInput[], char passInput[]){
  /*Declaração de variaveis locais da função validarLogin*/
  FILE *ficheiroAbrir;
  char *token, linha[35];
  bool findUser;
  ficheiroAbrir = fopen(nomeFicheiro, "r");
  /*Se conseguio abrir o ficheiro*/
  if(ficheiroAbrir != NULL){
    do{
      /*Lẽ linha do ficheiro*/
      findUser = false;
      fscanf(ficheiroAbrir,"%s",linha);
      /*Separa a linha até encontrar ':' e guarda em token*/
      token = strtok(linha, ":");
      /*Enquanto houve conteudo em token*/
      while(token != NULL){
        /*Se não encontrou user*/
        if(!findUser){  
          /*Compara token com utilizar*/      
          if(strcmp(token, utilizadorInput) == 0){ 
            /*Se for igual, indica que encontrou user*/
            findUser = true; 
          }
          /*E procura proximo bocado até ':'*/
          token = strtok(NULL, ":");
        }else{//Se já encontrou User          
          /*Compara token com a palavra-passe*/
          if(strcmp(token, passInput) == 0){ 
            /*Se forem iguais, significa que o user indicou
            corretamente a sua senha*/
            fclose(ficheiroAbrir);//Fecha o ficheiro
            return 1; /*<- Retorna 1, indicando que encontrou user e a senha está correta*/
          }else{
            /*Se a senha estiver incorreta*/
            fclose(ficheiroAbrir);//Fecha ficheiro
            return 2; /*<- Retorn 2, indicando que encontrou user, mas a senha estava incorreta*/
          }  
        }
      }
    }while(feof(ficheiroAbrir) == 0);//Faz ciclo enquanto NÃO chega ao fim de ficheiro
    fclose(ficheiroAbrir);//Fecha ficheiro
    return 0; /*<- Retorna 0, quando percorreu o ficheiro todo, mas não encontrou utilizador*/
  }else{
    return -1; /*<- Retorna -1, quand não consegue abrir o ficheiro*/ 
  }
}

bool resetJogadorPID(int pid){
  int i;
  for(i = 0; i < TAM_JOGADORES; i++){
    if(jogador[i].pid == pid){
      jogador[i].pid = 0;
      return true;
    }
  }
  return false;
}

void resetJogadores(){
  int i, count = 0;
  for(i = 0; i < TAM_JOGADORES; i++){
    if(i <=8){
      jogador[i].equipa = 0;
    }else{
      jogador[i].equipa = 1;
    }
    if(count < 5){
      jogador[i].idNum = count;      
    }else if(count >=5){
      jogador[i].idNum = count+1;
    }
    count++;
    if(count == 9){
      count = 0;
    }
    jogador[i].pid = 0;
    jogador[i].bola = false;
  }
  resetPosicaoInicial();
}

void resetPosicaoInicial(){
    jogador[0].ponto.y = 10;
    jogador[0].ponto.x = 2;
    jogador[0].bola = false;

    jogador[1].ponto.y = 5;
    jogador[1].ponto.x = 7;
    jogador[1].bola = false;
    jogador[2].ponto.y = 8;
    jogador[2].ponto.x = 10;
    jogador[2].bola = false;
    jogador[3].ponto.y = 13;
    jogador[3].ponto.x = 9;
    jogador[3].bola = false;
    jogador[4].ponto.y = 17;
    jogador[4].ponto.x = 8;
    jogador[4].bola = false;

    jogador[5].ponto.y = 3;
    jogador[5].ponto.x = 18;
    jogador[5].bola = false;
    jogador[6].ponto.y = 8;
    jogador[6].ponto.x = 20;
    jogador[6].bola = false;
    jogador[7].ponto.y = 13;
    jogador[7].ponto.x = 22;
    jogador[7].bola = false;
    jogador[8].ponto.y = 19;
    jogador[8].ponto.x = 18;
    jogador[8].bola = false;

    jogador[9].ponto.y = 10;
    jogador[9].ponto.x = 48;
    jogador[9].bola = false;

    jogador[10].ponto.y = 5;
    jogador[10].ponto.x = 41;
    jogador[10].bola = false;
    jogador[11].ponto.y = 8;
    jogador[11].ponto.x = 43;
    jogador[11].bola = false;
    jogador[12].ponto.y = 13;
    jogador[12].ponto.x = 41;
    jogador[12].bola = false;
    jogador[13].ponto.y = 17;
    jogador[13].ponto.x = 43;
    jogador[13].bola = false;

    jogador[14].ponto.y = 2;
    jogador[14].ponto.x = 35;
    jogador[14].bola = false;
    jogador[15].ponto.y = 6;
    jogador[15].ponto.x = 30;
    jogador[15].bola = false;
    jogador[16].ponto.y = 15;
    jogador[16].ponto.x = 28;
    jogador[16].bola = false;
    jogador[17].ponto.y = 18;
    jogador[17].ponto.x = 33;
    jogador[17].bola = false;
}

void printResult(){
  wattron(win_consola, A_BOLD | COLOR_PAIR(3));
  wprintw(win_consola, "[");
  wattroff(win_consola, A_BOLD | COLOR_PAIR(3));
  wattron(win_consola, A_BOLD);
  wprintw(win_consola, "ESQUERDA");
  wattroff(win_consola, A_BOLD);
  wattron(win_consola, A_BOLD | COLOR_PAIR(3));
  wprintw(win_consola, "] ");
  wattroff(win_consola, A_BOLD | COLOR_PAIR(3));
  wattron(win_consola, A_BOLD);
  wprintw(win_consola, "%d - %d", resultado_esquerda, resultado_direita);
  wattroff(win_consola, A_BOLD);
  wattron(win_consola, A_BOLD | COLOR_PAIR(2));
  wprintw(win_consola, " [");
  wattroff(win_consola, A_BOLD | COLOR_PAIR(2));
  wattron(win_consola, A_BOLD);
  wprintw(win_consola, "DIREITA");
  wattroff(win_consola, A_BOLD);
  wattron(win_consola, A_BOLD | COLOR_PAIR(2));
  wprintw(win_consola, "]\n");
  wattroff(win_consola, A_BOLD | COLOR_PAIR(2));
}

void terminaJogoClientes(){
  int i;
  pthread_join(thread_validaJogo, NULL);
  for(i = 0; i < TAM_JOGADORES; i++){
    if(jogador[i].pid != 0){
      terminarJogoaUmCliente(jogador[i].pid);
    }
  }
}

void terminarJogoaUmCliente(int pid){
  int fd;
  char fifoDoJogoCliente[25];
  sprintf(fifoDoJogoCliente, "fifo%d", pid);
  fd = open(fifoDoJogoCliente, O_WRONLY);
  write(fd, &jogador, sizeof(jogador));
  close(fd);
}

bool mudaJogador(int eq, int jog, int pid){
  int i;
  for(i = 0; i < TAM_JOGADORES; i++){
    if(jogador[i].equipa == eq && jogador[i].idNum == jog && jogador[i].pid == 0){
      jogador[i].pid = pid;
      return true;
    }
  }
  return false;
}

void atualizarScoreClientes(){
  union sigval valor_sinal;
  int i;
  for(i = 0; i < TAM_JOGADORES; i++){
    if(user[i].pid != 0){
      valor_sinal.sival_int = resultado_esquerda;
      if(sigqueue(user[i].pid, SIGPIPE, valor_sinal) != 0){
        wprinterror(win_consola, "Sinal não enviado.\n");
      }
      valor_sinal.sival_int = resultado_direita;
      if(sigqueue(user[i].pid, SIGURG, valor_sinal) != 0){
        wprinterror(win_consola, "Sinal não enviado.\n");
      }
    }
  }
}

int equipaBola(){
	int i;
	for(i = 0; i < TAM_JOGADORES; i++){
		if(jogador[i].bola){
			return jogador[i].equipa;
		}
	}
}