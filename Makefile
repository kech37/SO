so: servidor.c util.h cliente.c
	gcc servidor.c util.h -o serv -lncurses -lpthread
	gcc cliente.c util.h -o cli -lncurses -lpthread
	
