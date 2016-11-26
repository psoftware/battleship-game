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

/**** METODI PER LA INVIO RICEZIONE DEI DATI ****/
const int BYTE_LENGTH_COUNT=sizeof(unsigned int);

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

void print_str_eos(char * mul_str, int length)
{
	int i;
	for(i=0; i<length; i++)
		if(mul_str[i]=='\0')
			printf("|");
		else
			printf("%c", mul_str[i]);
	printf("\n");
}

int split_eos(char * mul_str, int length, char ** string_pointers, int max_pointers)
{
	string_pointers[0]=mul_str;

	int counter=1, i;
	for(i=0; i<length-1; i++)
		if(mul_str[i]=='\0')
		{
			string_pointers[counter]=&mul_str[i]+1;
			if(++counter>max_pointers)
				break;
		}

	return counter;
}

int pack_eos(char * mul_str, char ** string_pointers, int n_str)
{
	int i;
	int buff_next_index=0;
	for(i=0; i<n_str; i++)
	{
		strcpy(&mul_str[buff_next_index], string_pointers[i]);
		buff_next_index += strlen(string_pointers[i])+1;
	}

	return buff_next_index;
}