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

enum cl_status {WAIT_INIT, CONNECTING, WAIT_CONN_REPLY, READY, INGAME};

typedef struct des_clients
{
	int sock;
	char username[30];
	char address[15];
	char port[5];

	int req_conn_sock;		//in questo campo viene depositato il socket del client in attesa di connettersi o connesso
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

des_client * des_client_find_user(des_client *head, char * username)
{
	des_client *q;
	for(q=head; q!=NULL && strcmp(username, q->username)!=0; q=q->next);

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

void remove_client_nonotify(int cl_sock, fd_set *master)
{
	FD_CLR(cl_sock, master);
	close(cl_sock);
	des_client_remove(&client_list, cl_sock);
	printf("Client %d sconnesso!\n", cl_sock);
}

int cmd_disconnect_request(int cl_sock);
int cmd_reply_connect(int dest_sock, fd_set * master, int accepted);
void remove_client(int cl_sock, fd_set *master)
{
	//individuo il cl_des del socket che voglio chiudere
	des_client * cl_des = des_client_find(client_list, cl_sock);

	if(cl_des != 0)
		switch(cl_des->status)
		{
			case INGAME:								// il client stava giocando
				cmd_disconnect_request(cl_sock);		// va mandata una notifica di disconnessione al client con cui giocava
				break;
			case CONNECTING:							// il client voleva connettersi a qualcun'altro 
				cmd_disconnect_request(cl_sock);		// quindi mando una notifica di disconnessione al client con cui voleva giocare
				break;
			case WAIT_CONN_REPLY:						// il client ha ricevuto una connect, ma ancora non ha risposto
				cmd_reply_connect(cl_sock, master, 0);	// presumo che abbia risposto negativamente
				break;
			case READY: break; case WAIT_INIT: break;	//non devo fare niente in questo caso
		}

	remove_client_nonotify(cl_sock, master);
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
		if(q->status!=WAIT_INIT)
		{
			//printf("%s(%s)\n", q->username, (q->status==READY)?"libero":"occupato");
			strcat(str, "\t");
			strcat(str, q->username);
			strcat(str, "(");
			strcat(str, (q->status==READY)?"libero":"occupato");
			strcat(str, ")\n");
		}
}

int cmd_user(int cl_sock, char * buff, int buff_len)
{
	//print_str_eos(buff, buff_len);

	//individuo il cl_des del socket che ha mandato la richiesta user
	des_client * cl_des = des_client_find(client_list, cl_sock);
	if(!cl_des)
		return -1;

	//splitto il pacchetto che mi è arrivato
	char * strs[3];
	if(split_eos(buff, buff_len, strs, 3)!=3)
		return -1; //condizione da gestire!!!

	//controllo che l'username non esista già
	if(des_client_check_duplicate(client_list, cl_sock, strs[1]) == -1)
		return -2;

	//aggiorno i dati del cl_des
	strcpy(cl_des->username, strs[1]);
	strcpy(cl_des->port, strs[2]);
	cl_des->status = READY;

	printf("%s è libero\n", cl_des->username);

	return 1;
}

int cmd_connect(int cl_sock, char * buff, int buff_len, char * res_address, char * res_port)
{
	//print_str_eos(buff, buff_len);

	//individuo il cl_des del socket che ha mandato la richiesta
	des_client * cl_des = des_client_find(client_list, cl_sock);
	if(!cl_des)
		return -1; //descrittore non trovato

	//splitto il pacchetto che mi è arrivato
	char * strs[2];
	if(split_eos(buff, buff_len, strs, 2)!=2)
		return -1; //messaggio mal formato

	//se il client vuole connettersi a se stesso
	if(!strcmp(cl_des->username, strs[1]))
		return -2; //l'utente vuole connettersi a se stesso

	//prendo il descrittore del client a cui l'altro client vuole connettersi
	des_client * dest_des = des_client_find_user(client_list, strs[1]);
	if(!dest_des || dest_des->status==WAIT_INIT)
		return -3; //username non trovato
	if(dest_des->status==INGAME)
		return -4; //giocatore occupato
	if(dest_des->status==CONNECTING)
		return -5; //giocatore si sta connettendo a qualcun'altro
	if(dest_des->status==WAIT_CONN_REPLY)
		return -6; //qualcun'altro sta già chiedendo al giocatore di giocare

	//invio comando al client con cui il client vuole giocare
	char temp_buffer[50];
	char prefix[]="CONNECTREQ";
	char * params[4];
	params[0]=prefix; params[1]=cl_des->username; params[2]=cl_des->address; params[3]=cl_des->port;
	int size = pack_eos(temp_buffer, params, 4);
	int ret = send_variable_string(dest_des->sock, temp_buffer, size);
	if(ret == 0 || ret == -1)
		return -1;	//send fallita/client disconnesso

	//memorizzo nel descrittore che cl_client sta effettuando la connect e
	//che l'eventuale risposta deve essere indirizzata ad esso
	cl_des->status=CONNECTING;
	dest_des->status=WAIT_CONN_REPLY;
	dest_des->req_conn_sock = cl_sock;

	//mi devo tenere da parte il req_conn_sock perchè potrebbe avvenire una disconnect
	//prima che il client destinatario accetti/rifiuti, devo sapere a chi mandarla!
	cl_des->req_conn_sock= dest_des->sock;

	return 1;
}

//ATTENZIONE: la cmd_reply_connect è la seconda fase della cmd_connect.
//Il send_buffer non è tra i parametri perchè devo rispondere a req_conn_sock
//e non al socket che ha mandato la CONNECTIONACCEPTED/DECLINED.
int cmd_reply_connect(int dest_sock, fd_set * master, int accepted)
{
	char temp_buffer[100];
	int message_lenght;

	des_client * dest_des = des_client_find(client_list, dest_sock);
	if(!dest_des)
	{
		//printf("cmd_reply_connect: dest_des %d non trovato\n", dest_sock);
		return -1;	// errore generico
	}
	if(dest_des->status==READY)	//se sono in questa situazione significa che al cl_des si è disconnesso prima che ottenesse una risposta
	{
		//printf("cmd_reply_connect: Il client che ha fatto la richiesta si è disconnesso\n");
		return -2;	// condizione non gestita, meglio così
	}
	else if(dest_des->status!=WAIT_CONN_REPLY)
	{
		//printf("cmd_reply_connect: Messaggio non previsto/Errore generico\n");
		return -1;	// errore generico
	}

	des_client * cl_des = des_client_find(client_list, dest_des->req_conn_sock);
	if(!cl_des) // MANCA UNA CONDIZIONE (client in attesa)
	{
		//printf("cmd_reply_connect: cl_des non trovato\n");
		return -1;	// errore generico
	}

	if(accepted==1)
	{
		printf("%s si è connesso a %s!\n", cl_des->username, dest_des->username);

		cl_des->status=INGAME;
		dest_des->status=INGAME;

		cl_des->req_conn_sock=dest_sock;

		char prefix[]="CONNECTOK";
		char * params[3];
		params[0]=prefix; params[1]=dest_des->address; params[2]=dest_des->port;
		message_lenght = pack_eos(temp_buffer, params, 3);
	}
	else
	{
		cl_des->status=READY;
		dest_des->status=READY;
		//printf("Il client non vuole giocare!\n");
		strcpy(temp_buffer, "USERNOTACCEPTED");
		message_lenght=strlen("USERNOTACCEPTED")+1;
	}

	//la send è fatta su req_conn_sock, cioè il client che ha avviato il comando di connect (precedentemente)
	int ret = send_variable_string(dest_des->req_conn_sock, temp_buffer, message_lenght);
	if(ret==0 || ret==-1)
		remove_client(dest_des->req_conn_sock, master);	//attenzione, potrebbe essere ricorsiva

	return 1;
}

int cmd_disconnect_request(int cl_sock)
{
	des_client * cl_des = des_client_find(client_list, cl_sock);
	if(!cl_des) // MANCA UNA CONDIZIONE (client in attesa)
	{
		printf("cmd_disconnect_request: cl_des non trovato\n");
		return -1;
	}

	des_client * dest_des = des_client_find(client_list, cl_des->req_conn_sock);
	if(!dest_des)
	{
		printf("cmd_disconnect_request: dest_des %d non trovato\n", cl_des->req_conn_sock);
		return -1;
	}
	if(dest_des->status==WAIT_INIT || dest_des->status==READY)
	{
		printf("cmd_disconnect_request: dest_des %d è nello stato pronto oppure wait_init\n", cl_des->req_conn_sock);
		return -1;
	}


	int ret = send_variable_string(cl_des->req_conn_sock, "DISCONNECTNOTIFY", strlen("DISCONNECTNOTIFY")+1);
	if(ret==0 || ret==-1)
		return ret;

	cl_des->status=READY;
	dest_des->status=READY;
	printf("%s si è disconnesso da %s!\n", cl_des->username, dest_des->username);
	printf("%s é libero\n", cl_des->username);
	printf("%s é libero\n", dest_des->username);

	return 1;
}

int cmd_ilose_request(int cl_sock)
{
	des_client * cl_des = des_client_find(client_list, cl_sock);
	if(!cl_des) // MANCA UNA CONDIZIONE (client in attesa)
	{
		printf("cmd_ilose_request: cl_des non trovato\n");
		return -1;
	}

	des_client * dest_des = des_client_find(client_list, cl_des->req_conn_sock);
	if(!dest_des || dest_des->status!=INGAME)
	{
		printf("cmd_ilose_request: dest_des %d non trovato oppure non sta giocando\n", cl_des->req_conn_sock);
		return -1;
	}

	int ret = send_variable_string(cl_des->req_conn_sock, "WINNOTIFY", strlen("WINNOTIFY")+1);
	if(ret==0 || ret==-1)
		return ret;

	cl_des->status=READY;
	dest_des->status=READY;

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
	printf("Indirizzo: 0.0.0.0 (Porta: %d)\n\n", port);

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

		int message_lenght=0;

		for(i=0; i<=max_sock_num; i++)
			if(FD_ISSET(i, &read_fd))
			{
				//printf("Select risvegliata da socket %d\n", i);
				if(i==sock_serv)	//significa che è riuscita una accept
				{
					int my_len = sizeof(cl_addr);
					new_sd = accept(sock_serv, (struct sockaddr*)&cl_addr, (socklen_t*)&my_len);
					FD_SET(new_sd, &master);
					if(new_sd > max_sock_num)
						max_sock_num = new_sd;

					//creo nuovo client, ne inizializzo stato e socket e lo aggiungo alla lista globale
					des_client * new_cl = (des_client*)malloc(sizeof(des_client));
					new_cl->sock=new_sd;
					if(!inet_ntop(AF_INET, (struct sockaddr*)&cl_addr.sin_addr, new_cl->address, 15))
						perror("Errore conversione indirizzo client:");
					new_cl->status=WAIT_INIT;
					new_cl->next=NULL;
					strcpy(new_cl->username, "");
					des_client_add(&client_list, new_cl);

					printf("Client con ip %s connesso. In attesa di username e porta UDP!\n", new_cl->address);
				}
				else //altrimenti qualche client vuole mandarmi dati
				{
					//printf("Nuovi dati da socket %d!\n", i);
					int ret = recv_variable_string(i, rec_buffer);
					if(ret == 0 || ret == -1) {
						remove_client(i, &master);
						continue;
					}

					if(!strcmp(rec_buffer, "user")) {
						//printf("Il client ha inviato username e porta!\n");
						switch(cmd_user(i, rec_buffer, ret))
						{
							case -1:	//errore non recuperabile
								//printf("Errore non recuperabile\n");
								strcpy(send_buffer, "GENERICERROR");
								break;
							case -2:	//username duplicato
								//printf("Username già esistente\n");
								strcpy(send_buffer, "USEREXISTS");
								break;
							case 1:		//tutto ok!
								//printf("Dati corretti, il client ora è pronto\n");
								strcpy(send_buffer, "OK");
								break;
						}
					}
					else if(!strcmp(rec_buffer, "!who")) {
						//printf("Ricevuto who!\n");
						cmd_who(send_buffer);
					}
					else if(!strcmp(rec_buffer, "!connect")) {
						//printf("Ricevuto connect!\n");
						char res_address[30];
						char res_port[5];
						switch(cmd_connect(i, rec_buffer, ret, res_address, res_port))
						{
							case -1:	//errore non recuperabile
								//printf("Errore non recuperabile\n");
								strcpy(send_buffer, "GENERICERROR");
								break;
							case -2:
								//printf("L'utente vuole connettersi a se stesso (wtf?)\n");
								strcpy(send_buffer, "USERSELF");
								break;
							case -3:	//username duplicato
								//printf("Username non esistente\n");
								strcpy(send_buffer, "USERNOTFOUND");
								break;
							case -4:
								//printf("Il client sta già giocando\n");
								strcpy(send_buffer, "USERNOTREADY");
								break;
							case -5:
								//printf("Il client sta effettuando la connessione ad un altro giocatore\n");
								strcpy(send_buffer, "USERNOTREADY");
								break;
							case -6:
								//printf("Qualcuno ha già chiesto al client di giocare, anche se la richiesta è in attesa\n");
								strcpy(send_buffer, "USERNOTREADY");
								break;
							case 1:
								continue;	//la cmd_connect ha fatto la richiesta al client,
											//ora devo aspettare la sua risposta, ma al client
											//richiedente non devo mandare niente!
						}
					}
					else if(!strcmp(rec_buffer, "CONNECTACCEPT"))	// mando risposta al client che 
					{												// aveva fatto precedentemente richiesta (accettato)
						if(cmd_reply_connect(i, &master, 1)==-1)
							remove_client(i, &master);
						continue;									// Non è prevista risposta al client mittente
					}
					else if(!strcmp(rec_buffer, "CONNECTDECLINE"))	// mando risposta al client che 
					{												// aveva fatto precedentemente richiesta (rifiutato)
						if(cmd_reply_connect(i, &master, 0)==-1)
							remove_client(i, &master);
						continue;									// Non è prevista risposta al client mittente
					}
					else if(!strcmp(rec_buffer, "DISCONNECTREQ"))	// Il client si è arreso o si è disconnesso
					{
						if(cmd_disconnect_request(i)==-1)
							remove_client(i, &master);
						continue;									// Non è prevista risposta al client mittente
					}
					else if(!strcmp(rec_buffer, "ILOSEREQ"))		// Il client ha riconosciuto di aver perso
					{
						if(cmd_ilose_request(i)==-1)
							remove_client(i, &master);
						continue;									// Non è prevista risposta al client mittente
					}
					else
					{
						//printf("Comando non riconosciuto!\n");
						strcpy(send_buffer, "GENERICERROR");
					}
					//printf("Sto inviando %s\n", send_buffer);
					ret = send_variable_string(i, send_buffer, (message_lenght==0)?strlen(send_buffer)+1:message_lenght);
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

