all: server client

server: server.o commlib.o
	gcc server.o commlib.o -o server

client: client.o commlib.o
	gcc client.o commlib.o -o client

commlib.o: lib/commlib.c
	gcc -Wall -g -c lib/commlib.c -o commlib.o

server.o: server.c
	gcc -Wall -g -c server.c -o server.o

client.o: client.c
	gcc -Wall -g -c client.c -o client.o

clean:
	rm commlib.o server.o client.o server client
