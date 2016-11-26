#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<sys/types.h>
#include<sys/socket.h>

#include<arpa/inet.h>
#include<netinet/in.h>

#include<unistd.h>

const int BYTE_LENGTH_COUNT=sizeof(unsigned int);

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

int main(int argc, char * argv[])
{
	int porta;
	if(argc == 1)
	{
		printf("Porta non specificata!\n");
		return -1;
	}
	porta=atoi(argv[1]);

	printf("Porta: %d\n", porta);

	//(socket remoto, TCP, sempre zero)
	int sock_client = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in srv_addr;
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(porta);
	inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr);

	
	if(connect(sock_client, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1)
	{
		perror("Connect fallita");
		exit(1);
	}

	char buffer[1024];
	
	while(1)
	{
		// prompt console per comando
		printf("!");
		scanf("%s", buffer);

		// parso il comando
		if(strcmp(buffer, "help")==0)
		{
			printf("Stampo help...\n");
			continue;
		}
		if(strcmp(buffer, "bye")==0)
			break;
		if(!strcmp(buffer, "who") || !strcmp(buffer, "connect"))
		{
			int ret = send_variable_string(sock_client, buffer, strlen(buffer));
			if(ret == 0 || ret == -1)
				break;
		}
		else
			continue;

		// ricevo la risposta dal server
		int ret = recv_variable_string(sock_client, buffer);
		if(ret == 0 || ret == -1)
			break;
		buffer[ret]='\0';
		printf("Ricevuto: %s\n", buffer);
	}

	close(sock_client);
	return 0;
}

