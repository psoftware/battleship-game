#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <unistd.h>

#include <errno.h>

//libreria per il pack dei dati
#include "lib/commlib.h"


/*** STRUTTURE DATI ***/

enum cl_status {WAIT_INIT, READY, INGAME};

typedef struct des_clients
{
	int sock;
	char username[30];
	char * address[15];
	short int port;
	enum cl_status status;

	struct des_clients * next;
} des_client;

des_client *client_list=NULL;

/*** FUNZIONI PER LISTE ***/

void des_client_add(des_client **head, des_client *elem)
{
	elem->next = *head;
	*head=elem;
}

void des_client_remove(des_client **head, int cl_sock)
{
	//condizione lista vuota
	if(*head==NULL)
		return;

	des_client *q, *p=*head;
	for(q=*head; q->next!=NULL && cl_sock!=q->sock; q=q->next)
		p=q;

	if(q==0)	//elemento non trovato?? Condizione da gestire
		return;

	if(q==*head)
		*head=q->next;
	else
		p->next=q->next;

	//DEALLOCARE q!!
}

des_client * des_client_find(des_client *head, int cl_sock)
{
	des_client *q;
	for(q=head; q!=NULL && cl_sock!=q->sock; q=q->next);

	if(q==NULL)
		return NULL;

	return q;
}

int des_client_check_duplicate(des_client *head, int cl_sock, char * username)
{
	des_client *q;
	for(q=head; q!=NULL && (strcmp(q->username, username)!=0 || cl_sock==q->sock); q=q->next);

	if(q==NULL)
		return 0;
	return -1;
}

void remove_client(int cl_sock, fd_set *master)
{
	FD_CLR(cl_sock, master);
	close(cl_sock);
	des_client_remove(&client_list, cl_sock);
	printf("Client %d sconnesso!\n", cl_sock);
}

/**** INIZIALIZZAZIONE ****/

// questa funzione inizializza il socket, effettua la bind e anche la listen
int initialize_server_socket(const char * bind_addr, int port)
{
	int ret_sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in my_addr;
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	inet_pton(AF_INET, bind_addr, &my_addr.sin_addr);

	if(bind(ret_sock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1)
	{
		perror("Bind fallita");
		exit(1);
	}
	if(listen(ret_sock, 10) == -1)
	{
		perror("Listen fallita");
		exit(1);
	}

	return ret_sock;
}

/**** METODI PER COMANDI RICEVUTI ****/
void cmd_who(char * str)
{
	str[0]='\0';
	strcat(str, "Client connessi al server:\n");

	des_client *q;
	for(q=client_list; q!=NULL; q=q->next)
		if(q->status==INGAME || q->status==READY)
		{
			printf("%s(%s)\n", q->username, (q->status==INGAME)?"occupato":"libero");
			strcat(str, "\t");
			strcat(str, q->username);
			strcat(str, "(");
			strcat(str, (q->status==INGAME)?"occupato":"libero");
			strcat(str, ")\n");
		}
}

int cmd_user(int cl_sock, char * buff, int buff_len)
{
	print_str_eos(buff, buff_len);

	//individuo il cl_des del socket che ha mandato la richiesta user
	des_client * cl_des = des_client_find(client_list, cl_sock);
	if(!cl_des)
		return -1;

	//splitto il pacchetto che mi è arrivato
	char * strs[3];
	if(split_eos(buff, buff_len, strs, 5)!=3)
		return -1; //condizione da gestire!!!

	//controllo che l'username non esista già
	if(des_client_check_duplicate(client_list, cl_sock, strs[1]) == -1)
		return -2;

	//aggiorno i dati del cl_des
	strcpy(cl_des->username, strs[1]);
	cl_des->port =atoi(strs[2]);
	cl_des->status = READY;

	return 1;
}

/**** MAIN ****/

int main(int argc, char * argv[])
{
	int port;
	if(argc == 1)
	{
		printf("Porta non specificata!\n");
		exit(1);
	}
	port=atoi(argv[1]);

	// creo e bindo socket server
	int sock_serv = initialize_server_socket("0.0.0.0", port);

	struct sockaddr_in cl_addr;
	memset(&cl_addr, 0, sizeof(cl_addr));
	
	fd_set master;
	fd_set read_fd;
	int i, new_sd, max_sock_num=sock_serv;

	FD_ZERO(&master);
	FD_ZERO(&read_fd);
	FD_SET(sock_serv, &master);
	
	char rec_buffer[1024];
	char send_buffer[1024];

	while(1)
	{
		read_fd = master;
		select(max_sock_num+1, &read_fd, NULL, NULL, NULL);
		for(i=0; i<=max_sock_num; i++)
			if(FD_ISSET(i, &read_fd))
			{
				printf("Select risvegliata da socket %d\n", i);
				if(i==sock_serv)//significa che è riuscita una accept
				{
					int my_len = sizeof(cl_addr);
					new_sd = accept(sock_serv, (struct sockaddr*)&cl_addr, (socklen_t*)&my_len);
					FD_SET(new_sd, &master);
					if(new_sd > max_sock_num)
						max_sock_num = new_sd;

					//creo nuovo client, ne inizializzo stato e socket e lo aggiungo alla lista globale
					des_client * new_cl = (des_client*)malloc(sizeof(des_client));
					new_cl->sock=new_sd;
					new_cl->status=WAIT_INIT;
					new_cl->next=NULL;
					strcpy(new_cl->username, "");
					des_client_add(&client_list, new_cl);
					printf("Client connesso (socket %d). In attesa di dati!\n", new_sd);
				}
				else //altrimenti qualche client vuole mandarmi dati
				{
					printf("Nuovi dati da socket %d!\n", i);
					int ret = recv_variable_string(i, rec_buffer);
					if(ret == 0 || ret == -1) {
						remove_client(i, &master);
						continue;
					}

					if(!strcmp(rec_buffer, "user")) {
						printf("Il client ha inviato username e porta!\n");
						switch(cmd_user(i, rec_buffer, ret))
						{
							case -1:	//errore non recuperabile
								printf("Errore non recuperabile\n");
								strcpy(send_buffer, "GENERICERROR");
								break;
							case -2:	//username duplicato
								printf("Username già esistente\n");
								strcpy(send_buffer, "USEREXISTS");
								break;
							case 1:		//tutto ok!
								printf("Dati corretti, il client ora è pronto\n");
								strcpy(send_buffer, "OK");
								break;
						}
					}
					else if(!strcmp(rec_buffer, "!who")) {
						printf("Ricevuto who!\n");
						cmd_who(send_buffer);
					}
					
					//printf("Sto inviando %s\n", send_buffer);
					ret = send_variable_string(i, send_buffer, strlen(send_buffer)+1);
					if(ret == 0 || ret == -1) {
						remove_client(i, &master);
						continue;
					}
				}
			}
	}
	close(sock_serv);
	return 0;
}

