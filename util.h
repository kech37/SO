#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/select.h>
#include <ncurses.h>
#include <pthread.h>

#define NOME_FIFO_SERVIDOR "fifoServidor"
#define NOME_FIFO_SERVIDOR_JOGO "fifoJogo"

#define TAM_X 51
#define TAM_Y 21

#define TAM_JOGADORES 18


typedef struct{
    int y;
    int x;
}PONTO;

typedef struct{
   char name[255];
   bool logado;
   int pid;
}USER;

typedef struct{
    int equipa; // 0 - ESQ   1 - DIR
    int idNum; // 0 - G, 1 a 4 - D, 6 a 9 - A
    int pid;
    bool bola;
    PONTO ponto; //Posicao
}JOGADOR;

typedef struct{
	USER user;
	char comando[25];
	int tecla;
}PEDIDO;

typedef struct{
  int tempo;
  int r_direita;
  int r_esquerda;
  int j_logados;
}INFO;