#!/bin/bash
rm commlib.o server.o client.o server client

gcc -Wall -c lib/commlib.c -o commlib.o

gcc -Wall -c server.c -o server.o
gcc server.o commlib.o -o server

gcc -Wall -c client.c -o client.o
gcc client.o commlib.o -o client
