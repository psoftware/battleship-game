#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <unistd.h>

#include <errno.h>

#include <ctype.h>

//libreria per il pack dei dati
#include "lib/commlib.h"

#define GRID_SIZE 6
#define SHIP_COUNT 7
#define USERNAME_MAXL 60
#define PORT_MAXL 40

#define UDPCONSOLE_TIMEOUT 60

typedef enum { false, true } bool;

enum client_status {TCPCOMM, INGAME, WAIT_UDP_STATUS, WAIT_UDP_COORDS};

char buffer[1024];

void print_help()
{
	printf("Sono disponibili i seguenti comandi:\n");
	printf("!help --> mostra l'elenco dei comandi disponibili\n");
	printf("!who --> mostra l'elenco dei client connessi al server\n");
	printf("!connect username --> avvia una partita con l\'utente username\n");
	printf("!quit --> disconnette il client dal server\n");
}

int initialize_udp_server(int port)
{
	//(socket remoto, TCP, sempre zero)
	int sock_udp = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in my_addr;
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	inet_pton(AF_INET, "0.0.0.0", &my_addr.sin_addr);

	if(bind(sock_udp, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1)
	{
		perror("Bind UDP fallita:");
		exit(1);
	}

	return sock_udp;
}

enum my_area_status {WATER, HITWATER, SHIP, DIEDSHIP};
enum my_area_status my_grid[GRID_SIZE][GRID_SIZE];
int my_success_hit = 0;

enum enemy_area_status {UNKNOWN, NOHIT, HIT};
enum enemy_area_status enemy_grid[GRID_SIZE][GRID_SIZE];
int enemy_success_hit = 0;

int ismyturn=0;

void print_game_help()
{
	printf("Sono disponibili i seguenti comandi:\n");
	printf("!help --> mostra l'elenco dei comandi disponibili\n");
	printf("!disconnect --> disconnette il client dall'attuale partita\n");
	printf("!shot square --> fai un tentativo con la casella square\n");
	printf("!show --> visualizza griglia di gioco \n");
}

void clear_grids()
{
	int i, j;
	for(i=0; i<GRID_SIZE; i++)
		for(j=0; j<GRID_SIZE; j++)
		{
			my_grid[i][j]=WATER;
			enemy_grid[i][j]=UNKNOWN;
		}

	my_success_hit=0;
	enemy_success_hit=0;
}

void print_hor_delim()
{
	int j;
	printf("  ");
	for(j=0; j<GRID_SIZE+1; j++)
		printf("--");
	for(j=0; j<5; j++)
		printf("  ");
	printf("  ");
	for(j=0; j<GRID_SIZE+1; j++)
		printf("--");
	printf("\n");
}

void show_grids()
{
	int i, j;

	print_hor_delim();

	for(i=0; i<GRID_SIZE; i++)
	{
		printf("%d |", i);
		for(j=0; j<GRID_SIZE; j++)
		{
			char toprint;
			switch(my_grid[i][j])
			{
				case WATER: toprint=' '; break;
				case HITWATER: toprint='~'; break;
				case SHIP: toprint='$'; break;
				case DIEDSHIP: toprint='X'; break;
			}
			printf("%c ", toprint);
		}

		printf("|");
		for(j=0; j<5; j++)
				printf("  ");

		printf("%d |", i);
		for(j=0; j<GRID_SIZE; j++)
		{
			char toprint;
			switch(enemy_grid[i][j])
			{
				case UNKNOWN: toprint=' '; break;
				case NOHIT: toprint='~'; break;
				case HIT: toprint='X'; break;
			}
			printf("%c ", toprint);
		}
		printf("|");
		printf("\n");
	}

	print_hor_delim();

	printf("   ");
	for(j=0; j<GRID_SIZE; j++)
		printf("%c ", 'A'+j);
	for(j=0; j<5; j++)
		printf("  ");
	printf("    ");
	for(j=0; j<GRID_SIZE; j++)
		printf("%c ", 'A'+j);
	printf("\n");
}

int check_text_position(char * str)
{
	if(strlen(str)!=2)
		return -1;
	if(!isalpha(str[0]) || !isdigit(str[1]))
		return -1;
	if(str[0]-'a' >= GRID_SIZE || str[1]-'0' >= GRID_SIZE)
		return -1;

	return 1;
}

enum my_area_status * get_my_coords(char * str)
{
	if(check_text_position(str)==-1)
		return NULL;

	str[0] = tolower(str[0]);
	return &my_grid[ (int)(str[1]-'0') ][ (int)(str[0]-'a') ];
}

enum enemy_area_status * get_enemy_coords(char * str)
{
	if(check_text_position(str)==-1)
		return NULL;

	str[0] = tolower(str[0]);
	return &enemy_grid[ (int)(str[1]-'0') ][ (int)(str[0]-'a') ];
}

int place_ship(char * str)
{
	if(check_text_position(str)==-1)
		return -1;

	enum my_area_status * area = get_my_coords(str);
	if(*area!=WATER)
		return -2;

	*area=SHIP;

	return 1;
}

void ask_ships_position()
{
	printf("Posiziona %d caselle nel formato <lettera><numero> (es. a2):\n", SHIP_COUNT);
	int i=0;
	char str[60];
	while(i!=SHIP_COUNT)
	{
		printf("Coordinata: ");
		scanf("%s", str);
		if(check_text_position(str)==-1)
			printf("Hai inserito una cordinata errata (%s), riprova.\n", str);
		else
		{
			if(place_ship(str)==-2)
				printf("E' già presente una nave su quella casella, riprova.\n");
			else
			{
				i++;
				if(i!=SHIP_COUNT)	//salta quando non mancano più navi
					printf("Nave posizionata, mancano altre %d navi\n", SHIP_COUNT-i);
			}
		}
	}
}

void other_client_coords_turn(int sock_client_udp, struct sockaddr_in udp_srv_addr)
{
	printf("E' il turno dell'avversario!\n");
	//ricevo messaggio udp con cordinate
	char coords[3];
	int ret = udp_receive_coords(sock_client_udp, udp_srv_addr, &coords[0], &coords[1]);
	if(ret==0 || ret==-1)
	{
		printf("Ricezione UDP non riuscita (ricezione coordinate)!\n");
		return;
	}
	coords[2]='\0';

	enum response_code resp;
	if(check_text_position(coords)==-1)
	{
		printf("La posizione inviata dal client è invalida!\n");
		resp=R_ERROR;
	}

	enum my_area_status * my_area = get_my_coords(coords);
	if(*my_area==DIEDSHIP || *my_area==HITWATER)
	{
		printf("Il client ha già colpito questa posizione!\n");
		resp=R_ERROR;
	}
	else if(*my_area==WATER)
	{
		printf("Il client ha colpito %s, ma c'era acqua\n", coords);
		*my_area=HITWATER;
		resp=R_NOTHIT;
	}
	else if(*my_area==SHIP)
	{
		printf("Il client ha colpito e affondato %s\n", coords);
		enemy_success_hit++;
		*my_area=DIEDSHIP;
		resp=R_HIT;
	}
	
	//invio lo stato solo se non ho perso, altrimenti procederò con una richiesta di disconnessione (vedi nel multiplexing udp)
	if(enemy_success_hit==SHIP_COUNT)
		return;

	//invio lo stato al client
	ret = udp_send_response_status(sock_client_udp, udp_srv_addr, resp);
	if(ret==0 || ret==-1)
	{
		printf("Invio UDP non riuscito (invio errore)!\n");
		return;
	}

	printf("E' il tuo turno!\n");
}

enum enemy_area_status * game_cmd_shot(int sock_client_udp, struct sockaddr_in udp_srv_addr)
{
	char str[40];
	scanf("%s", str);

	if(check_text_position(str)==-1)
	{
		printf("La posizione inserita è invalida!\n");
		return 0;
	}

	enum enemy_area_status * enemy_area = get_enemy_coords(str);
	if(*enemy_area!=UNKNOWN)
	{
		printf("Hai già colpito questa posizione!\n");
		return 0;
	}

	//invia messaggio udp con cordinate
	int ret = udp_send_coords(sock_client_udp, udp_srv_addr, str[0], str[1]);
	if(ret==0 || ret==-1)
	{
		printf("Invio UDP non riuscito (invio coordinate)!\n");
		return 0;
	}

	return enemy_area;
}

void game_shot_response(int sock_client_udp, struct sockaddr_in udp_srv_addr, enum enemy_area_status * enemy_area, char * enemy_username)
{
	//ricevi nuovo stato
	enum response_code resp;
	int ret = udp_receive_response_status(sock_client_udp, udp_srv_addr, &resp);
	if(ret==0 || ret==-1 || resp==R_ERROR)
	{
		printf("Ricezione UDP non riuscita (ricezione stato)!\n");
		return;
	}

	if(resp==R_HIT)
	{
		my_success_hit++;
		*enemy_area=HIT;
		printf("%s dice: colpito! :)\n\n", enemy_username);
	}
	else if(resp==R_NOTHIT)
	{
		*enemy_area=NOHIT;
		printf("%s dice: mancato :(\n\n", enemy_username);
	}
}

struct sockaddr_in init_udp_game(int sock_client_udp, char * address, char * port, int isfirst)
{
	// fase di connessione al server udp (l'altro client)
	struct sockaddr_in udp_srv_addr;

	memset(&udp_srv_addr, 0, sizeof(udp_srv_addr));
	udp_srv_addr.sin_family = AF_INET;
	udp_srv_addr.sin_port = htons(atoi(port));
	inet_pton(AF_INET, address, &udp_srv_addr.sin_addr);

	clear_grids();
	ask_ships_position();

	if(isfirst==0)
		printf("\nE' il turno dell'avversario!\n");

	//se inizio prima posso passare alla console, altrimenti devo
	//attendere la mossa dell'altro client
	/*if(isfirst==0)
		other_client_coords_turn(sock_client_udp, udp_srv_addr);
	else
		printf("E' il tuo turno!\n");*/

	return udp_srv_addr;
}

int cmd_connect(int sock_client, char * username, char * res_address, char * res_port)
{
	char prefix_str[]="!connect";

	//pacco i dati inseriti e li invio
	char * params[2]; params[0]=prefix_str; params[1]=username;
	int size = pack_eos(buffer, params, 2);
	int ret = send_variable_string(sock_client, buffer, size);
	if(ret == 0 || ret == -1)
	{
		close(sock_client);
		return 0;
	}

	//attendo la risposta del server
	size = recv_variable_string(sock_client, buffer);
	if(ret == 0 || ret == -1)
	{
		close(sock_client);
		return 0;
	}

	//printf("Connect Ricevuto: ");
	//print_str_eos(buffer, size);

	if(!strcmp(buffer, "GENERICERROR"))
		return -1;
	else if(!strcmp(buffer, "USERSELF"))
		return -2;
	else if(!strcmp(buffer, "USERNOTFOUND"))
		return -3;
	else if(!strcmp(buffer, "USERNOTREADY"))
		return -4;
	else if(!strcmp(buffer, "USERNOTACCEPTED"))
		return -5;
	else if(!strcmp(buffer, "CONNECTOK")) {
		//splitto il pacchetto che mi è arrivato
		char * strs[3];
		int strs_num = split_eos(buffer, size, strs, 3);
		if(strs_num!=3)
		{
			printf("Ho ricevuto meno stringhe: %d\n", strs_num);
			return -1; //messaggio mal formato
		}
		
		//Restituisco le stringhe ricevute (indirizzo e porta)
		strcpy(res_address, strs[1]);
		strcpy(res_port, strs[2]);

		return 1;
	}

	return -1; //in teoria non ci si dovrebbe arrivare qui, quindi nel caso segnalo errore generico
}

int cmd_disconnect(int sock_client)
{
	int ret = send_variable_string(sock_client, "DISCONNECTREQ", strlen("DISCONNECTREQ")+1);
	return ret;
}

int cmd_timeoutudp(int sock_client)
{
	int ret = send_variable_string(sock_client, "TIMEOUTUDP", strlen("TIMEOUTUDP")+1);
	return ret;
}

int cmd_you_win(int sock_client)
{
	int ret = send_variable_string(sock_client, "ILOSEREQ", strlen("ILOSEREQ")+1);
	return ret;
}

void set_stdin_select(fd_set* fd, bool set)
{
	if(!FD_ISSET(0, fd) && set)
		FD_SET(0, fd);
	else if(FD_ISSET(0, fd) && !set)
		FD_CLR(0, fd);
}

int main(int argc, char * argv[])
{
	int porta, ret;
	if(argc != 3)
	{
		printf("Parametri non corretti!\n");
		printf("La sintassi è ./battleclient <host remoto> <porta>\n");
		exit(1);
	}
	//porta=atoi(argv[2]);
	porta=check_port_str(argv[2]);
	if(porta==-1)
	{
		printf("Numero di porta non valido! Range valido 0-65535\n");
		exit(1);
	}

	// creo il socket TCP
	int sock_client = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in srv_addr;
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(porta);
	inet_pton(AF_INET, argv[1], &srv_addr.sin_addr);
	
	// effettuo la connect al server indicato
	if(connect(sock_client, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1)
	{
		perror("Connect fallita");
		exit(1);
	}

	printf("\nConnessione al server %s (porta %d) effettuata con successo\n\n", argv[1], porta);
	print_help();
	
	// ------ prima fase: richiesta di inserimento username e porta d'ascolto
	int udp_port;
	while(1)
	{
		//richiedo username e porta
		char prefix_str[]="user";
		char user_str[USERNAME_MAXL];
		char port_str[PORT_MAXL];

		strcpy(prefix_str, "user");
		printf("\nInserisci il tuo nome: ");
		scanf("%s", user_str);

		while(1)
		{
			printf("Inserisci la porta UDP di ascolto: ");
			scanf("%s", port_str);
			udp_port=check_port_str(port_str);
			if(udp_port==-1)
				printf("Numero di porta non valido! Range valido 0-65535\n");
			else
				break;
		}

		//pacco i dati inseriti e li invio
		char * params[3]; params[0]=prefix_str; params[1]=user_str; params[2]=port_str;
		int size = pack_eos(buffer, params, 3);
		ret = send_variable_string(sock_client, buffer, size);
		if(ret == 0 || ret == -1)
		{
			close(sock_client);
			return 0;
		}

		// ottengo una risposta (dati invalidi oppure meno)
		ret = recv_variable_string(sock_client, buffer);
		if(ret == 0 || ret == -1)
		{
			close(sock_client);
			return 0;
		}
		//printf("Ricevuto: %s\n", buffer);

		if(!strcmp(buffer, "OK"))
			break;
		else if(!strcmp(buffer, "USEREXISTS"))
			printf("\nUn utente si è già registrato con questo nome, scegline un altro");
		else if(!strcmp(buffer, "GENERICERROR"))
		{
			printf("\nSi è verificato un errore non risolvibile, termino.");
			exit(1);
		}
		else
		{
			printf("\nRisposta non prevista, termino.");
			printf("\n");
			exit(1);
		}
	}

	//setto il server udp, mi metterò in ascolto dopo
	int sock_udp = initialize_udp_server(udp_port);

	//definisco il sockaddr che utilizzero per memorizzare l'indirizzo del client a cui manderò i dati UDP
	struct sockaddr_in udp_srv_addr;

	//mi tengo da parte l'username del client con cui voglio giocare, dopo aver fatto la connect
	char enemy_username[USERNAME_MAXL];

	//definisco una variabile che conterrà la casella che sto shootando, visto che la risposta UDP è multiplexata
	enum enemy_area_status * shooting_area;

	// ------ seconda fase: invio comandi a server tcp e ricezione di richieste di gioco
	fd_set master, read_fd;
	FD_ZERO(&master);
	FD_ZERO(&read_fd);
	set_stdin_select(&master, true);
	FD_SET(sock_client, &master);
	FD_SET(sock_udp, &master);

	struct timeval tv;

	//questa variabile booleana mi serve per evitare che venga ristampato il carattere della console quando scatta il timeout
	bool skip_cmdchar=false;

	enum client_status cl_stat=TCPCOMM;
	for(;;)
	{
		// prompt console per comando
		if(cl_stat==WAIT_UDP_STATUS || cl_stat==WAIT_UDP_COORDS)
		{
			set_stdin_select(&master, false);	//disabilito temporaneamente lo sblocco su select causato da stdin
			skip_cmdchar=false;					//se dovevo skippare il carattere della console ora non devo più farlo
		}
		else
		{
			if(!skip_cmdchar)
			{
				if(cl_stat==0)
					printf("> ");
				else
					printf("# ");
			}
			else	//il carattere deve essere skippato solo una volta
				skip_cmdchar=false;

			fflush(stdout);
			set_stdin_select(&master, true);
		}

		read_fd = master;
		tv.tv_sec = UDPCONSOLE_TIMEOUT;
		tv.tv_usec = 0;
		select((sock_client>sock_udp) ? sock_client+1 : sock_udp+1, &read_fd, NULL, NULL, &tv);
		if(FD_ISSET(0, &read_fd))
		{

			scanf("%s", buffer);

			if(cl_stat==TCPCOMM)
			{
				// parso il comando
				if(!strcmp(buffer, "!quit"))
					break;
				else if(!strcmp(buffer, "!help"))
				{
					print_help();
					continue;
				}
				else if(!strcmp(buffer, "!connect"))
				{
					char username[USERNAME_MAXL];
					scanf("%s", username);
					char res_address[30];
					char res_port[PORT_MAXL];
					printf("Mi sto connettendo a %s\n", username);
					switch(cmd_connect(sock_client, username, res_address, res_port))
					{
						case -1:
							printf("Errore generico!\n");
							continue;
						case -2:
							printf("Non puoi giocare con te stesso!\n");
							continue;
						case -3:
							printf("L'utente inserito non esiste!\n");
							continue;
						case -4:
							printf("L'utente inserito già sta giocando!\n");
							continue;
						case -5:
							printf("L'utente inserito ha rifiutato la tua richiesta!\n");
							continue;
						case 1:
							printf("Connessione riuscita!\n");
							cl_stat=INGAME;
							strcpy(enemy_username, username);
							udp_srv_addr = init_udp_game(sock_udp, res_address, res_port, 1);
							continue;
					}
				}
				else if(!strcmp(buffer, "!who"))
				{
					ret = send_variable_string(sock_client, "!who", 5);
					if(ret == 0 || ret == -1)
						break;
				}
				else
				{
					printf("Il comando inserito è errato!\n");
					continue;
				}

				// ricevo la risposta dal server (non ho bisogno di multiplexare perchè ricevo risposte a comandi inviati)
				ret = recv_variable_string(sock_client, buffer);
				if(ret == 0 || ret == -1)
					break;
				buffer[ret]='\0';
				printf("%s", buffer);
				printf("\n");
			}
			else if(cl_stat==INGAME)
			{
				if(!strcmp(buffer, "!help")) {
					print_game_help();
				}
				else if(!strcmp(buffer, "!disconnect")) {
					cmd_disconnect(sock_client);	//mando in TCP la richiesta di disconnessione
					cl_stat=TCPCOMM;
				}
				else if(!strcmp(buffer, "!shot")) {
					shooting_area=game_cmd_shot(sock_udp, udp_srv_addr);
					if(shooting_area!=0)
						cl_stat=WAIT_UDP_STATUS;
				}
				else if(!strcmp(buffer, "!show")) {
					show_grids();
				}
				else
					printf("Il comando inserito è errato!\n");
			}
			else
				printf("Sono in uno stato in cui non è previsto che ricevi roba nella scanf\n");
		}
		else if(FD_ISSET(sock_client, &read_fd))
		{
			ret = recv_variable_string(sock_client, buffer);
			if(ret == 0 || ret == -1)
					break;

			//printf("Il server ha mandato dati sul socket tcp senza che glieli abbia chiesti!\n");
			//printf("Ha mandato: ");
			//print_str_eos(buffer,ret);

			//splitto il pacchetto che mi è arrivato
			char * strs[4];
			int strs_count = split_eos(buffer, ret, strs, 4);

			if(!strcmp(strs[0], "CONNECTREQ")) {
				if(strs_count!=4)	//controllo che mi siano state mandate almeno 5 stringhe
					printf("Formato risposta invalido (Errore generico)\n");

				//chiedo se l'utente vuole giocare
				char resp;
				printf("%s vuole giocare con te, vuoi accettare? (y/n): ", strs[1]);
				while(1) {
					scanf(" %c", &resp);
					if(resp=='y' || resp=='n')
						break;
					else
						printf("\nDevi rispondere y oppure n: ");
				}

				//comunico la mia risposta al server (che la reindirizzerà al client)
				if(resp=='y') {
					ret = send_variable_string(sock_client, "CONNECTACCEPT", 14);
					strcpy(enemy_username, strs[1]);
					udp_srv_addr = init_udp_game(sock_udp, strs[2], strs[3], 0);
					cl_stat=WAIT_UDP_COORDS;
				}
				else
					ret = send_variable_string(sock_client, "CONNECTDECLINE", 15);
			}
			else if(!strcmp(strs[0], "DISCONNECTNOTIFY")){
				if(cl_stat!=TCPCOMM)
				{
					printf("\n** Il client si è arreso. Hai vinto la partita!\n");
					cl_stat=TCPCOMM;
				}
			}
			else if(!strcmp(strs[0], "WINNOTIFY")){
				if(cl_stat!=TCPCOMM)
				{
					printf("\n** Hai vinto la partita!\n");
					cl_stat=TCPCOMM;
				}
			}
			else if(!strcmp(strs[0], "TIMEOUTNOTIFY")){
				if(cl_stat!=TCPCOMM)
				{
					printf("\n** Tempo per la risposta scaduto, hai perso la partita!\n");
					cl_stat=TCPCOMM;
				}
			}
			else
				printf("Comando non riconosciuto: %s", strs[0]);
		}
		else if(FD_ISSET(sock_udp, &read_fd))	//RECEIVE SOCKET UDP
		{
			//printf("Select sbloccata dal socket udp!\n");
			if(cl_stat==WAIT_UDP_STATUS)
			{
				game_shot_response(sock_udp, udp_srv_addr, shooting_area, enemy_username);
				cl_stat=WAIT_UDP_COORDS;
			}
			else if(cl_stat==WAIT_UDP_COORDS)
			{
				other_client_coords_turn(sock_udp, udp_srv_addr);
				if(enemy_success_hit==SHIP_COUNT)		// se tutte le mie navi sono state affondate allora
				{										// provvedo a notificare la mia sconfitta.
					printf("\n** Hai perso la partita!\n\n");
					cmd_you_win(sock_client);	// NON E' DETTO CHE LA RISPOSTA UDP ARRIVI SEMPRE PRIMA DELLA TCP!
					cl_stat=TCPCOMM;
				}
				else
					cl_stat=INGAME;
			}
			else
			{
				printf("Risposta udp ricevuta fuori sync!\n");
				break;
			}
		}
		else // è scattato il timer
			if(cl_stat==INGAME)		//se è il mio turno significa che mi arrendo
			{
				printf("\n** Tempo scaduto, hai perso!\n\n");
				cmd_disconnect(sock_client);
				cl_stat=TCPCOMM;
			}
			else if(cl_stat==WAIT_UDP_COORDS || cl_stat==WAIT_UDP_STATUS)		//significa che non ho avuto una risposta UDP in sufficiente tempo
			{
				printf("\n** Tempo per la risposta dell'avversario scaduto!\n\n");
				cmd_timeoutudp(sock_client);
				cl_stat=TCPCOMM;
			}
			else
				skip_cmdchar=true;
	}

	close(sock_client);
	close(sock_udp);
	return 0;
}

