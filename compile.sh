#!/bin/bash
rm server.o client.o server client

gcc -Wall -c server.c -o server.o
gcc server.o -o server

gcc -Wall -c client.c -o client.o
gcc client.o -o client
