# Battleship Game - A TCP/UDP Console Game
A simple implementation of BattleShip game using sockets programming and C.

### Design
A centralized TCP server (battle_server.c) manages clients registration and matchmaking requests. After pairing two users, clients (battle_client.c) play using a direct UDP socket.
Two players can be matched using a request-response mechanism. Interaction with users is made through a simple console interface with commands (launch the !help command to get more details).

### Implementation details
The Central Server is not using multithreading but it uses I/O Multiplexing through the select() system call. Complex client requests are handled using a state system approach, thus to minimize waiting time for all the other clients.
lib/ folder contains common functions for string parsing and complex parameter passing. All methods are carefully implemented to support mixed client-server system architectures.
