FILES:

- client.c : used to run the client
- client.h : header file for the client
- server.c : used to run the server
- server.h : header file for ther server
- common.c : contains functions used by both the server and the client
- common.h : header file for the common file + defines macros used in both the server and the client

TO COMPILE ALL FILES:

Type "make" in the command line.

TO RUN THE SERVER:

To start the server from the terminal, enter: ./server <port_number>
Once the server is started, it waits to receive messages from clients.
There is one thread per client.
The server prints in the terminal the history of sent (with the tag [sent]) and received (with the tag [recv]) messages.
The server prints in the terminal when a game starts, when a game ends, or when a game is aborted (stopped in the middle).
The server can handle two games at a time, so at most 4 clients grouped in pairs of 2. 
If a 5th client comes in and all games are full, the client is rejected. The client can join once one of the games is over. 
The number of concurrent games can be modified by changing the macro MAX_GAMES defined in server.c
The server runs forever unless the process gets interrupted. 

If the user doesn't follow the input/command line formats/requirements, an error message helps him correct his input. 

TO RUN THE CLIENT:

Make sure the server is running. 
To start a client from the terminal, enter: ./client <ip_address> <port_number>
Once the client is started, it waits for a second client to announce itself to the server.
Once the game starts, the client prompts the user to input his or her move on the following format : row column
Row and column must be 0 or 1 or 2. 
The user can quit the game by entering a q instead of a valid move.
The program stops when there is a win, a draw, or one of the players left the game.

If the user doesn't follow the input/command line formats/requirements, an error message helps him correct his input. 


