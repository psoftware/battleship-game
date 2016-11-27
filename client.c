#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <unistd.h>

#include <errno.h>

//libreria per il pack dei dati
#include "lib/commlib.h"

char buffer[1024];

void print_help()
{
	printf("Sono disponibili i seguenti comandi:\n");
	printf("!help --> mostra l'elenco dei comandi disponibili\n");
	printf("!who --> mostra l'elenco dei client connessi al server\n");
	printf("!connect username --> avvia una partita con l\'utente username\n");
	printf("!quit --> disconnette il client dal server\n");
}

int cmd_connect(int sock_client, char * username)
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

	printf("Connect Ricevuto: ");
	print_str_eos(buffer, size);

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
		
		//FARE COSE UDP
		return 1;
	}

	return -1; //in teoria non ci si dovrebbe arrivare qui, quindi nel caso segnalo errore generico
}

int main(int argc, char * argv[])
{
	int porta, ret;
	if(argc != 3 )
	{
		printf("Parametri non corretti!\n");
		printf("La sintassi è ./battleclient <host remoto> <porta>\n");
		exit(1);
	}
	porta=atoi(argv[2]);

	//(socket remoto, TCP, sempre zero)
	int sock_client = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in srv_addr;
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(porta);
	inet_pton(AF_INET, argv[1], &srv_addr.sin_addr);

	
	if(connect(sock_client, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1)
	{
		perror("Connect fallita");
		exit(1);
	}

	printf("\nConnessione al server %s (porta %d) effettuata con successo\n\n", argv[1], porta);
	print_help();
	
	// ------ prima fase: richiesta di inserimento username e porta d'ascolto
	while(1)
	{
		//richiedo username e porta
		char prefix_str[]="user";
		char user_str[20];
		char port_str[20];

		strcpy(prefix_str, "user");
		printf("\nInserisci il tuo nome: ");
		scanf("%s", user_str);
		printf("Inserisci la porta UDP di ascolto: ");
		scanf("%s", port_str);

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
		printf("Ricevuto: %s\n", buffer);

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

	// ------ seconda fase: invio comandi a server tcp e ricezione di richieste di gioco
	fd_set master, read_fd;
	FD_ZERO(&master);
	FD_ZERO(&read_fd);
	FD_SET(0, &master);
	FD_SET(sock_client, &master);

	while(1)
	{
		// prompt console per comando
		printf("> ");
		fflush(stdout);

		read_fd = master;
		select(sock_client+1, &read_fd, NULL, NULL, NULL);
		if(FD_ISSET(0, &read_fd))
		{
			scanf("%s", buffer);

			// parso il comando
			if(!strcmp(buffer, "!quit"))
				break;
			if(!strcmp(buffer, "!help"))
			{
				print_help();
				continue;
			}
			if(!strcmp(buffer, "!connect"))
			{
				char username[20];
				scanf("%s", username);
				switch(cmd_connect(sock_client, username))
				{
					case -1:
						printf("Errore generico!\n");
						break;
					case -2:
						printf("Non puoi giocare con te stesso!\n");
						break;
					case -3:
						printf("L'utente inserito non esiste!\n");
						break;
					case -4:
						printf("L'utente inserito già sta giocando!\n");
						break;
					case -5:
						printf("L'utente inserito ha rifiutato la tua richiesta!\n");
						break;
					case 1:
						printf("Connessione riuscita!\n");
						break;
				}
			}
			if(!strcmp(buffer, "!who"))
			{
				ret = send_variable_string(sock_client, "!who", 5);
				if(ret == 0 || ret == -1)
					break;
			}
			else
				continue;

			// ricevo la risposta dal server
			ret = recv_variable_string(sock_client, buffer);
			if(ret == 0 || ret == -1)
				break;
			buffer[ret]='\0';
			printf("%s", buffer);
			printf("\n");
		}
		else if(FD_ISSET(sock_client, &read_fd))
		{
			ret = recv_variable_string(sock_client, buffer);
			if(ret == 0 || ret == -1)
					break;

			printf("Il server ha mandato dati sul socket tcp senza che glieli abbia chiesti!\n");
			printf("Ha mandato: ");
			print_str_eos(buffer,ret);

			//splitto il pacchetto che mi è arrivato
			char * strs[4];
			int strs_count = split_eos(buffer, ret, strs, 4);

			if(!strcmp(strs[0], "CONNECTREQ")) {
				if(strs_count!=4)	//controllo che mi siano state mandate almeno 5 stringhe
					printf("Formato risposta invalido (Errore generico)\n");

				//chiedo se l'utente vuole giocare
				char resp;
				printf("%s vuole giocare con te, vuoi accettare? (y/n): ", strs[1]);
				while(1)
				{
					scanf(" %c", &resp);
					if(resp=='y' || resp=='n')
						break;
					else
						printf("\nDevi rispondere y oppure n: ");
				}

				//comunico la mia risposta al server (che la reindirizzerà al client)
				if(resp=='y')
					ret = send_variable_string(sock_client, "CONNECTACCEPT", 14);
				else
					ret = send_variable_string(sock_client, "CONNECTDECLINE", 15);

				//mi metto, eventualmente, in modalità gioco (UDP)
				//MANCA
			}
			else
			{
				printf("Comando non riconosciuto: %s", strs[0]);
			}
		}
	}

	close(sock_client);
	return 0;
}

