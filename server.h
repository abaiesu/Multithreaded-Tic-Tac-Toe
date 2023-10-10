#ifndef SERVER_H
#define SERVER_H

#include <netdb.h>
#include <pthread.h>


struct message{
	char* text; //the actual message
	int sender; //the port of the sender
	struct sockaddr_in ad; //the address of the sender (to send the reply)
};


struct game_session {
    int ports[2]; // stores the port numbers of the clients in the game
    int num_clients; //stores the number of clients in the game
    int turn; // keeps track of each player's turn
    int full; // tells if a game is full (2 players)
    int game_over; // indicates if the game is over
    char fyi[29]; // store the FYI message
    int board[9]; // store the play board
    pthread_cond_t cond1; // condition for: is the game full/can we start?
    pthread_cond_t cond2; // condition for: is it our turn to play?
	pthread_t threads[2]; //to store the threads for each client
};


/*
@brief : treats the requests of a specific client

@param port : pointer to the port of the client
*/
void* treat_request(void *port);

/*
@brief : tells if there is a winning position 

@param board : board (array of 9 digits)

@return : -1 = no winner, 1 = player 1, 2 = player 2
*/
int is_position_winning(int* board);

/*
@brief : to be used if something went wrong (error in sending)
sends a TXT messages to the client and a LFT message 

@param arg : arguments for sending messages

@return : 0 = success, 1 = something went wrong
*/
int signal_error(struct arguments* arg);


#endif