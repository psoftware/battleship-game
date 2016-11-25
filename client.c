#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<sys/types.h>
#include<sys/socket.h>

#include<arpa/inet.h>
#include<netinet/in.h>

#include<unistd.h>

int main(int argc, char * argv[])
{
	const int BYTE_RECEIVE_COUNT=20;

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

	char buffer[BYTE_RECEIVE_COUNT];
	
	while(1)
	{
		printf("> ");
		scanf("%s", buffer);
		if(strcmp(buffer, "bye")==0)
		{
			close(sock_client);
			break;
		}
		int ret = send(sock_client, (void*)buffer, BYTE_RECEIVE_COUNT, 0);
		if(ret < strlen(buffer))
			printf("Byte inviati minori di quelli previsti!\n");

		ret = recv(sock_client, (void*)buffer, BYTE_RECEIVE_COUNT, MSG_WAITALL);
		if(ret < BYTE_RECEIVE_COUNT)
			printf("Byte ricevuti (%d) minori di quelli previsti!\n", ret);

		printf("Ricevuto: %s\n", buffer);
	}
	return 0;
}

