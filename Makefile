all: battle_server battle_client

battle_server: battle_server.o commlib.o
	gcc battle_server.o commlib.o -o battle_server

battle_client: battle_client.o commlib.o
	gcc battle_client.o commlib.o -o battle_client

commlib.o: lib/commlib.c
	gcc -Wall -g -c lib/commlib.c -o commlib.o

battle_server.o: battle_server.c
	gcc -Wall -g -c battle_server.c -o battle_server.o

battle_client.o: battle_client.c
	gcc -Wall -g -c battle_client.c -o battle_client.o

clean:
	rm commlib.o battle_server.o battle_client.o battle_server battle_client
