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

void print_help()
{
	printf("Sono disponibili i seguenti comandi:\n");
	printf("!help --> mostra l'elenco dei comandi disponibili\n");
	printf("!who --> mostra l'elenco dei client connessi al server\n");
	printf("!connect username --> avvia una partita con l\'utente username\n");
	printf("!quit --> disconnette il client dal server\n");
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
	
	char buffer[1024];
	
	// ------ prima fase: richiesta di inserimento username e porta d'ascolto
	while(1)
	{
		int buff_next_index=0;
		strcpy(buffer, "user");
		buff_next_index += strlen(buffer)+1;

		char temp_scanf[20];

		printf("\nInserisci il tuo nome: ");
		scanf("%s", temp_scanf);
		strcpy(&buffer[buff_next_index], temp_scanf);
		buff_next_index += strlen(temp_scanf)+1;

		printf("Inserisci la porta UDP di ascolto: ");
		scanf("%s", temp_scanf);
		strcpy(&buffer[buff_next_index], temp_scanf);
		buff_next_index += strlen(temp_scanf)+1;

		// invio username e porta
		ret = send_variable_string(sock_client, buffer, buff_next_index-1);
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
			printf("\nRisposta non riconosciuta, termino.");
			printf("\n");
			exit(1);
		}
	}

	// ------ seconda fase: invio comandi a server tcp
	while(1)
	{
		// prompt console per comando
		printf("> ");
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
		}
		if(!strcmp(buffer, "!who"))
		{
			ret = send_variable_string(sock_client, buffer, strlen(buffer)+1);
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

	close(sock_client);
	return 0;
}

