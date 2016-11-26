#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<sys/time.h>
#include<sys/types.h>
#include<sys/socket.h>

#include<arpa/inet.h>
#include<netinet/in.h>

#include<unistd.h>

#include<errno.h>

const int BYTE_LENGTH_COUNT=sizeof(unsigned int);
const int BYTE_RECEIVE_COUNT=20;

/*** STRUTTURE DATI ***/

enum cl_status {WAIT_INIT, READY, INGAME};

typedef struct des_clients
{
	int sock;
	char * address[15];
	short int port;
	enum cl_status status;

	struct des_clients * next;
} des_client;

des_client *client_list=NULL;

/*** FUNZIONI PER LISTE ***/

void des_client_add(des_client **head, des_client *elem)
{
	des_client *q, *p=*head;
	for(q=*head; q!=NULL; q=q->next)
		p=q;

	if(p==*head)
		*head=elem;
	else
		p=elem;
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

/**** METODI PER LA INVIO RICEZIONE DEI DATI ****/

int recv_variable_string(int cl_sock, char * buff)
{
	//faccio una recv di un byte
	unsigned int bytes_count;
	int ret = recv(cl_sock, (void*)&bytes_count, BYTE_LENGTH_COUNT, MSG_WAITALL);
	if(ret == 0 || ret == -1)
		return ret;

	//faccio una recv di nbyte ricevuti dalla recv precedente
	ret = recv(cl_sock, (void*)buff, bytes_count, MSG_WAITALL);
	if(ret == 0 || ret == -1)
		return ret;
	if(ret < bytes_count)
	{
		printf("recv_variable_string: Byte ricevuti (%d) minori di quelli previsti!\n", ret);
		return -1;
	}

	return bytes_count;
}

int send_variable_string(int cl_sock, char * buff, int bytes_count)
{
	//faccio una send del numero di byte che devo spedire
	int ret = send(cl_sock, (unsigned int*)&bytes_count, BYTE_LENGTH_COUNT, 0);
	if(ret == 0 || ret == -1)
		return ret;

	//faccio una send per i bytes_count bytes da inviare
	ret = send(cl_sock, (void*)buff, bytes_count, 0);
	if(ret == 0 || ret == -1)
		return ret;
	if(ret < bytes_count)
	{
		printf("send_variable_string: Byte ricevuti (%d) minori di quelli previsti!\n", ret);
		return -1;
	}
	return ret;
}

/**** METODI PER COMANDI RICEVUTI ****/
void who(char * str)
{
	str[0]='\0';
	strcat(str, "Lista\n");
	strcat(str, "HI\n");
	strcat(str, "HI\n");
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
	//int len = sizeof(my_addr);

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
					rec_buffer[ret]='\0';

					if(!strcmp(rec_buffer, "who")) {
						printf("Ricevuto who!\n");
						who(send_buffer);
					}
					else if(!strcmp(rec_buffer, "who")) {

					}

					ret = send_variable_string(i, send_buffer, strlen(send_buffer));
					if(ret == 0 || ret == -1) {
						remove_client(i, &master);
						continue;
					}
					/*
					int ret = recv(i, (void*)buffer, BYTE_RECEIVE_COUNT, MSG_WAITALL);
					if(ret == 0 || ret == -1)
						remove_client(i, &master);
					else
					{
						if(ret < BYTE_RECEIVE_COUNT)
							printf("Byte ricevuti (%d) minori di quelli previsti!\n", ret);

						ret = send(i, (void*)buffer, BYTE_RECEIVE_COUNT, 0);
						if(ret == 0 || ret == -1)
							remove_client(i, &master);
						if(ret < BYTE_RECEIVE_COUNT)
							printf("Byte inviati (%d) minori di quelli previsti!\n", ret);

						printf("Dati inviati correttamente!\n");
					}
					*/
				}
			}
	}
	close(sock_serv);
	return 0;
}

