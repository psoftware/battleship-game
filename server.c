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
	int sock_serv = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in my_addr;
	struct sockaddr_in cl_addr;
	memset(&my_addr, 0, sizeof(my_addr));
	memset(&cl_addr, 0, sizeof(cl_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(porta);
	inet_pton(AF_INET, "0.0.0.0", &my_addr.sin_addr);
	//my_addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(sock_serv, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1)
	{
		perror("Bind fallita");
		exit(1);
	}
	if(listen(sock_serv, 10) == -1)
	{
		perror("Listen fallita");
		exit(1);
	}
	
	fd_set master;
	fd_set read_fd;
	int i, new_sd, max_sock_num=sock_serv;

	FD_ZERO(&master);
	FD_ZERO(&read_fd);
	FD_SET(sock_serv, &master);
	
	char buffer[BYTE_RECEIVE_COUNT];
	//int len = sizeof(my_addr);

	while(1)
	{
		read_fd = master;
		select(max_sock_num+1, &read_fd, NULL, NULL, NULL);
		printf("Select sbloccata!\n");
		for(i=0; i<=max_sock_num; i++)
			if(FD_ISSET(i, &read_fd))
			{
				printf("Beccato socket %d\n", i);
				if(i==sock_serv)//significa che è riuscita una accept
				{
					//int ret = recv(new_sd, (void*)buffer, BYTE_RECEIVE_COUNT, 0);
					//printf("Ricevuti %d DEBUG!\n", ret);

					int my_len = sizeof(cl_addr);
					new_sd = accept(sock_serv, (struct sockaddr*)&cl_addr, (socklen_t*)&my_len);
					FD_SET(new_sd, &master);
					if(new_sd > max_sock_num)
						max_sock_num = new_sd;
					printf("Socket %d accettato!\n", new_sd);
				}
				else
				{
					printf("Nuovi dati da socket %d!\n", i);
					int ret = recv(i, (void*)buffer, BYTE_RECEIVE_COUNT, MSG_WAITALL);
					if(ret == 0 || ret == -1)
					{
						close(i);
						FD_CLR(i, &master);
						printf("Client %d sconnesso!\n", i);
					}
					else
					{
						if(ret < BYTE_RECEIVE_COUNT)
							printf("Byte ricevuti (%d) minori di quelli previsti!\n", ret);

						ret = send(i, (void*)buffer, BYTE_RECEIVE_COUNT, 0);
						if(ret == 0 || ret == -1)
						{
							close(i);
							FD_CLR(i, &master);
							printf("Client %d sconnesso!\n", i);
						}
						else
							if(ret < BYTE_RECEIVE_COUNT)
								printf("Byte inviati (%d) minori di quelli previsti!\n", ret);

						printf("Dati inviati correttamente!\n");
					}
				}
			}
		//FD_ZERO(&read_fd);
	}
	//close(new_sd);
	close(sock_serv);
	//printf("\n");
	return 0;
}

