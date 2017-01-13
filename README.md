<h1> SO - Trabalho Prático - ISEC</h1>

<h2>Comandos</2>

<h3>Cliente</h3>
- <b>login</b> <i>username password</i><br/>
O utilizador username pede para ser reconhecido pelo sistema, autenticando-se pela senha
password. Se o username e password indicados forem reconhecidos, o utilizador fica
habilitado a interagir com a restante funcionalidade do sistema.

- <b>join</b> <i>equipa jogador</i><br/>
Ao inicializar um jogo o utilizador vai ter a informação que um novo jogo está a decorrer,
para o utilizador se juntar ao jogo tem que indicar a equipa e o número do jogador que
pretende controlar.

- <b>logout</b><br/>
Indica ao servidor que este utilizador não deseja continuar a participar no sistema. O
servidor deixa de reconhecer o utilizador até que este volte a efectuar login.

<h3>Servidor</h3>
- <b>user</b> <i>username password</i><br/>
Introduz um novo cliente ao ficheiro que o servidor consulta para a permissão do login. Só os
clientes que estão registados é que tem acesso ao servidor

- <b>users</b><br/>
Apresenta uma lista completa dos clientes ligados e informa se já estão logados ou se
simplesmente estão ligados ao sistema.

- <b>shutdown</b><br/>
Termina o processo do servidor assim como todos os clientes que a ele estão ligados.

- <b>start</b> <i>temp</i><br/>
Inicializa um novo jogo, e avisa todos os clientes que estão logados que está em execução
um novo jogo para o caso dos clientes queiram jogar.

- <b>result</b><br/>
Mostra a pontuação do jogo que está a decorrer naquele momento.

- <b>matriz</b><br/>
Apresenta uma lista completa com os PIDs dos jogadores que estão a jogar. Caso o jogador
esteja a ser manipulado pelo computador o PID será 0.

- <b>stop</b><br/>
Termina um jogo que esteja em execução.

- <b>red</b> <i>nome</i><br/>
Quando é introduzido este comando o cliente deixa de estar logado no servidor.
Para que este cliente queira jogar novamente terá que fazer novamente login.
