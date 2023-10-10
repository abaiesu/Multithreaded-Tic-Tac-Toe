#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "common.h"
#include "server.h"

#define MAX_GAMES 2

//declare variables that will be accessed by all threads

pthread_mutex_t lock;
struct message* msg; //to store the message we receive
int sockfd; //to store the socket number
struct game_session games[MAX_GAMES];


int main(int argc, char* argv[]){

    if (argc != 2) {
        printf("Invalid number of arguments. Usage : ./server <port_number>\n");
        return 1; 
    }
    
    int ret; //a variable keeping the return values of functions (to check of errors)

    //initialize the mutex 
    if (pthread_mutex_init(&lock, NULL) != 0) {
        perror("Mutex initialization failed");
        return 1;
    }

    //get the port on which we have to listen on 
    char *port = argv[1];

    //build the address
    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    ad.sin_port = htons(atoi(port));
    ad.sin_addr.s_addr = INADDR_ANY;

    //create the socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        return 1;
    }

    //bind the socket
    ret = bind(sockfd, (const struct sockaddr *)&ad, sizeof(ad));
    if (ret == -1) {
        perror("Error in binding");
        close(sockfd);
        return 1;
    }

    //announce that we are ready to start
    printf("Server is ready\n\n");

    //initialize the variable that will store the received messages
    msg = malloc(sizeof(struct message));
    msg->text = NULL;

    //initialize the game sessions message
    int i,j;
    for(i=0; i< MAX_GAMES; i++){
        games[i].fyi[0] = (char) FYI; //FYI header
        games[i].fyi[1] = (char) 0; //number of filled positions
        for (j = 0; j < 9; j++) {
            games[i].board[j] = 0;
        }
        games[i].num_clients = 0;
        games[i].turn = 0; 
        games[i].full = -1; 
        games[i].game_over = -1;
        pthread_cond_init(&games[i].cond1, NULL);
        pthread_cond_init(&games[i].cond2, NULL);
    }

    while(1){ //wait for new messages

        pthread_mutex_lock(&lock); //lock the access to the shared variables

        for(i = 0; i< MAX_GAMES; i++){ //check for all games if one of them needs to be aborted
            
            if(games[i].full == 1 && games[i].num_clients != 2){ //one of the threads exited in the middle of the game
                
                games[i].num_clients = 0; //reset the number of clients
                games[i].full = -1; //reset full
                games[i].turn = !(games[i].turn); //flip turns so the second thread tells its player to leave too
                pthread_cond_signal(&(games[i].cond2)); //switch turns so the other player can see that we must quit and quit too
                printf(RED"    GAME %d ABORTED : A PLAYER LEFT"RESET, i);
                printf("\n");
            }
        }

        pthread_mutex_unlock(&lock); //lock the access to the shared variables
        
        if(msg->text == NULL){ 

            char buffer[SIZE]; //to store the new message
            socklen_t addrlen = sizeof(ad);
            int len = recvfrom(sockfd , buffer, SIZE-1, 0, (struct sockaddr *)&ad, &addrlen); //receive pack of data through the socket
            if(len < 0){
                perror("Error when receiving the data");
                continue; //wait for a new message
            }

            buffer[len] = '\0'; //terminate the received message (the full lenght is 100, we usully need to termiante the string before) 

            pthread_mutex_lock(&lock); //lock the access to the shared variables

            //store the received message in our msg
            msg->text = buffer;

            //now get the sender
            int sender = ntohs(ad.sin_port);
            msg->sender = sender;

            //and get the address it came from 
            msg->ad = ad;

            if((msg->text)[0] == TXT && strcasecmp((msg->text)+1, "hello")==0){ //TXT: a new client connected and said hello
                
                printf(CYAN "[recv] "RESET);
                printf("[TXT] from new player, port %d : %s\n", msg->sender, msg->text);

                //check for all games
                int found_free_game = -1;
                for(i = 0; i< MAX_GAMES; i++){

                    if(found_free_game != -1){ //we already found a free game
                        break;
                    }

                    
                    if(games[i].num_clients < 2){//we found space in a game

                        found_free_game = i;

                        if(games[i].num_clients == 0){ //the game didn't start yet

                            //put all the variables to the default values
                            games[i].turn = 0;
                            games[i].full = -1;
                            games[i].game_over = -1;
                            int j;
                            for(j = 0; j<9; j++){ //empty the board
                                games[i].board[j] = 0;
                            }
                            for(j = 0; j<29; j++){ //empty the FYI
                                games[i].fyi[j] = (char)0;
                            }
                            games[i].fyi[0] = (char) FYI; //add the header
                        }

                        //create a thread only for that client and give as argument the client port he's resposible for 
                        if (pthread_create(&(games[i].threads[games[i].num_clients]), NULL, treat_request, (void*)&sender)) {
                            perror("Error creating thread");
                            pthread_mutex_unlock(&lock);
                            msg->text = NULL;
                            continue; //if failure, wait for another message
                        }
                        
                        else{
                            games[i].ports[games[i].num_clients] = sender; //add the new port 
                            games[i].num_clients++; //increase the number of clients if it all went well
                        }

                        if(games[i].num_clients==2){ //the game can start
                            printf(GREEN"    GAME %d STARTS\n"RESET, i);
                            games[i].full = 1; //the game is full
                            pthread_cond_signal(&(games[i].cond1)); //signal to all the sleeping threads that they can start
                        }

                        msg->text = NULL;
                    }
                }


                if(found_free_game == -1){ //all the games are full
                   
                    //create the transport arguments
                    struct arguments* arg = malloc(sizeof(struct arguments));
                    arg->sockfd = sockfd;
                    arg->ad = ad;

                    //send the END message
                    int w = 0xFF; //to signal that the game is full
                    int ret = send_message(arg, (char*)&w, END);
                    if (ret == 1){ //error checking
                        perror("Error in sending the data\n");
                        pthread_mutex_unlock(&lock);
                        return 1;
                    }

                    printf(YELLOW"[sent] "RESET);
                    printf("[extra player] ");
                    printf("[END] \n");
                    msg->text = NULL;
                    free(arg);
                    pthread_mutex_unlock(&lock);
                    continue; //now wait for another message
                }

            }  
    
            pthread_mutex_unlock(&lock); //release the mutex
        }
       
    }

    pthread_mutex_destroy(&lock);
	
    close(sockfd);

    return 0;
}



void* treat_request(void* port){


    int ret; //a variable keeping the return values of functions (to check of errors)

    int own_port = *(int *) port; //take the port we're responsible for

    //let us find out the index of the game and of the player
    int game_index, player_index;
    int found;
    int i,j;
    pthread_mutex_lock(&lock);
    for(i = 0; i < MAX_GAMES; i++){
        for(j= 0; j< 2; j++){
            if(games[i].ports[j] == own_port){
                game_index = i;
                player_index = j;
                found = 1;
                break;
            }
        }
        if(found){
            break;
        }
    }

    //create the arguments passed to the send_message function
    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    ad.sin_port = htons(own_port);
    ad.sin_addr = (msg->ad).sin_addr;
    struct arguments* arg = malloc(sizeof(struct arguments));
    if(arg == NULL){
        games[game_index].num_clients--;
        pthread_mutex_unlock(&lock);
        pthread_exit(NULL);
    }
    arg->sockfd = sockfd;
    arg->ad = ad;

    //greet the client
    char greet[100];
    sprintf(greet, "Welcome! You are player %d in game %d. You play with: o", player_index + 1, game_index);
    ret = send_message(arg, (char*)greet, TXT);
    if(ret == 1){ //error checking
        games[game_index].num_clients--;
        pthread_mutex_unlock(&lock);
        signal_error(arg);
        pthread_exit(NULL);
    }
    printf(YELLOW"[sent] "RESET);
    printf("[TXT] to player %d in game %d : %s\n", player_index+1, game_index, greet);

    char* for_client;

    if(games[game_index].num_clients != 2){ //if not enough players to start the game
        
        for_client = "Waiting for a second player...";
    
        ret = send_message(arg, for_client, TXT);
        if(ret == 1){ //error checking
            games[game_index].num_clients--;
            pthread_mutex_unlock(&lock);
            signal_error(arg);
            pthread_exit(NULL);
        }
        printf(YELLOW"[sent] "RESET);
        printf("[TXT] to player %d in game %d : %s\n", player_index+1, game_index, for_client);

        while(games[game_index].num_clients != 2){ //while we wait for another client...
            pthread_cond_wait(&(games[game_index].cond1), &lock); //release the mutex to allow other threads to change full
        }
    
    }

    //now we start the game, let the client know
    for_client = "GAME STARTS";
    ret = send_message(arg, for_client, TXT); //send the message to the client
    if(ret == 1){ //error checking
        games[game_index].num_clients--;
        pthread_mutex_unlock(&lock);
        signal_error(arg);
        pthread_exit(NULL);
    }
    printf(YELLOW"[sent] "RESET);
    printf("[TXT] to player %d in game %d : %s\n", player_index+1, game_index, for_client);


    pthread_mutex_unlock(&lock);

    while(1){ //while the game is not ended
        
        pthread_mutex_lock(&lock);
    
        //check if it's our turn
        if(games[game_index].turn != player_index){

            for_client = "Waiting for the opponent to play...";
            ret = send_message(arg, for_client, TXT); //send the message to the client
            if(ret == 1){ //error checking
                games[game_index].num_clients--;
                pthread_mutex_unlock(&lock);
                signal_error(arg);
                pthread_exit(NULL);
            }
            printf(YELLOW"[sent] "RESET);
            printf("[TXT] to player %d in game %d : %s\n", player_index+1, game_index, for_client);

            while(games[game_index].turn != player_index){
                pthread_cond_wait(&(games[game_index].cond2), &lock); //wake up only once it's our turn
            }
        }

        //if the game is over 
        if(games[game_index].game_over >= 0){
            //show the board one last time
            ret = send_message(arg, games[game_index].fyi, FYI);
            if(ret == 1){ //error checking
                games[game_index].num_clients--;
                pthread_mutex_unlock(&lock);
                signal_error(arg);
                pthread_exit(NULL);
            }
            printf(YELLOW"[sent] "RESET);
            printf("[FYI] to player %d in game %d\n", player_index+1, game_index);
            
            //send an END message with the winner
            ret = send_message(arg, (char*)&(games[game_index].game_over), END); 
            if(ret == 1){ //error checking
                games[game_index].num_clients--; 
                pthread_mutex_unlock(&lock);
                signal_error(arg);
                pthread_exit(NULL);
            }
            printf(YELLOW"[sent] "RESET);
            printf("[END] to player %d in game %d\n", player_index+1, game_index);
            games[game_index].num_clients--; //remove the client from the players
            pthread_mutex_unlock(&lock);
            printf(RED "    GAME %d ENDS"RESET, game_index);
            printf("\n");
            free(arg);
            pthread_exit(NULL);
        }

        //if the game is not over, check that we still have 2 clients connected
        if(games[game_index].num_clients != 2){
            for_client = "The other player left";
            ret = send_message(arg, for_client, TXT); //send a text message to explain what happened
            if(ret==1){
                pthread_mutex_unlock(&lock);
                signal_error(arg);
                pthread_exit(NULL);
            }
            printf(YELLOW"[sent] "RESET);
            printf("[TXT] to player %d in game %d : %s\n", player_index+1, game_index, for_client);
            ret = send_message(arg, NULL, LFT); //signal to the client that he should leave the game
            if(ret==1){
                pthread_mutex_unlock(&lock);
                signal_error(arg);
                pthread_exit(NULL);
            }
            printf(YELLOW"[sent] "RESET);
            printf("[LFT] to player %d in game %d\n", player_index+1, game_index);
            pthread_mutex_unlock(&lock);
            free(arg);
            pthread_exit(NULL);
        }

        //if everything is fine, send a FYI message (to allow the player to see the board before making a move)
        ret = send_message(arg, games[game_index].fyi,FYI);
        if(ret == 1){
            games[game_index].num_clients--;
            pthread_mutex_unlock(&lock);
            signal_error(arg);
            pthread_exit(NULL);
        }
        
        //pthread_mutex_unlock(&lock);

        //ask for a move
        ret = send_message(arg, NULL, MYM);
        if(ret == 1){ //error checking
            games[game_index].num_clients--;
            pthread_mutex_unlock(&lock);
            signal_error(arg);
            pthread_exit(NULL);
        }
        printf(YELLOW"[sent] "RESET);
        printf("[MYM] to player %d in game %d\n", player_index+1, game_index);
    
        pthread_mutex_unlock(&lock); //unlock to allow modifications to msg


        while(1){ //until we get a valid position
        
            pthread_mutex_lock(&lock);

            //now the server should have received a message, so check if the message is for us
            if(msg->sender == own_port){ 

                if((msg->text)!= NULL){

                    if((int)(msg->text)[0] == LFT){ //if the client exited
                        games[game_index].num_clients--; //remove the client
                        printf(CYAN"[recv] "RESET);
                        printf("[LFT] from player %d in game %d\n", player_index+1, game_index);
                        pthread_mutex_unlock(&lock);
                        free(arg);
                        msg->text = NULL;
                        pthread_exit(NULL);
                    }
                    
                    if((int)(msg->text)[0] == MOV){ //check if it's a move message
                    
                        int col, row, board_p, fyi_p;
                        col = (int)(msg->text)[1];
                        row = (int)(msg->text)[2];

                        printf(CYAN"[recv] "RESET);
                        printf("[MOV] from player %d in game %d : col = %d, row = %d\n", player_index+1, game_index, col, row);

                        board_p = row*3 + col;

                        if(games[game_index].board[board_p] != 0){ //check if the position is occupied
                            
                            //tell the client that the position isn't valid
                            for_client = "Sorry, the position is already occupied, try again";
                            ret = send_message(arg, for_client, TXT);
                            if(ret == 1){
                                games[game_index].num_clients--;
                                pthread_mutex_unlock(&lock);
                                signal_error(arg);
                                pthread_exit(NULL);
                            }
                            printf(YELLOW"[sent] "RESET);
                            printf("[TXT] to player %d in game %d : %s\n", player_index+1, game_index, for_client);
                            

                            //send a new MYM message
                            ret = send_message(arg, NULL, MYM);
                            if(ret == 1){
                                games[game_index].num_clients--;
                                pthread_mutex_unlock(&lock);
                                signal_error(arg);
                                pthread_exit(NULL);
                            }
                            printf(YELLOW"[sent] "RESET);
                            printf("[MYM] to player %d in game %d\n", player_index+1, game_index);
                            
                            msg->text = NULL; //disable the message so we don't loop over it
                            pthread_mutex_unlock(&lock); 
                            continue; //wait again for a response
                        }
                    

                        games[game_index].board[board_p] = player_index+1; //place the move on the board
                        
                        fyi_p = 2 + games[game_index].fyi[1]*3; //get the position in the FYI message

                        games[game_index].fyi[1]++; //update number of filled positions
                        
                        //add the new position
                        games[game_index].fyi[fyi_p] = player_index+1; 
                        fyi_p++;
                        games[game_index].fyi[fyi_p] = col;
                        fyi_p++;
                        games[game_index].fyi[fyi_p] = row;

                        pthread_mutex_unlock(&lock); 
                        msg->text = NULL; //set the text to null to signal that it has been treated and that we can receive a new one
                        break; //exit the loop

                    }

                }
            
            }

            pthread_mutex_unlock(&lock); 
        
        }

        
        pthread_mutex_lock(&lock);
        
        games[game_index].turn = !(games[game_index].turn); //flip turns
        pthread_cond_signal(&(games[game_index].cond2)); //signal that we can switch turns

        games[game_index].game_over = is_position_winning(games[game_index].board); //check for winner

        if(games[game_index].game_over > 0){ //we have a winning position
        
            //show the last status of the board
            printf(YELLOW"[sent] "RESET);
            printf("[FYI] to player %d in game %d\n", player_index+1, game_index);
            ret = send_message(arg, games[game_index].fyi, FYI);
            if(ret == 1){
                games[game_index].num_clients--;
                pthread_mutex_unlock(&lock);
                signal_error(arg);
                pthread_exit(NULL);
            }
            
            //send the END message
            ret = send_message(arg,(char*)&(games[game_index].game_over), END);
            if(ret == 1){
                games[game_index].num_clients--;
                pthread_mutex_unlock(&lock);
                signal_error(arg);
                pthread_exit(NULL);
            }
            printf(YELLOW"[sent] "RESET);
            printf("[END] to player %d in game %d\n", player_index+1, game_index);
            
            games[game_index].num_clients--; //decrease the number of connected clients
            games[game_index].full = -1; //signal that we arn't full anymore
            pthread_mutex_unlock(&lock);
            free(arg);
            pthread_exit(NULL);
        }

        if(games[game_index].fyi[1] == 9){ //no winning position, but the board is full 
            
            games[game_index].game_over = 0; //set game over to 0 : draw

            //show the last status of the board
            printf(YELLOW"[sent] "RESET);
            printf("[FYI] to player %d in game %d\n", player_index+1, game_index);
            ret = send_message(arg, games[game_index].fyi, FYI);
            if(ret == 1){
                games[game_index].num_clients--;
                pthread_mutex_unlock(&lock);
                signal_error(arg);
                pthread_exit(NULL);
            }

            //and send the END message
            ret = send_message(arg, (char*)&(games[game_index].game_over), END);
            if(ret == 1){
                games[game_index].num_clients--;
                pthread_mutex_unlock(&lock);
                signal_error(arg);
                pthread_exit(NULL);
            }
            printf(YELLOW"[sent] "RESET);
            printf("[END] to player %d in game %d\n", player_index+1, game_index);
            
            games[game_index].num_clients--; //decrease the number of connected clients
            games[game_index].full = -1; //signal that we arn't full anymore
            pthread_mutex_unlock(&lock);
            free(arg);
            pthread_exit(NULL);
        }

        pthread_mutex_unlock(&lock);  

    }
    free(arg);
    
    pthread_exit(NULL);

}


int is_position_winning(int* board){

    int i, j;

   
    //check rows
    for(i = 0; i<3; i++){
        if(board[i*3] == board[i*3 + 1] && board[i*3] == board[i*3 + 2]){
            if(board[i*3] == 1 || board[i*3] == 2){
                return board[i*3];
            }
        }
    }

    //check columns
    for(j = 0; j<3; j++){
        if(board[j] == board[3 + j] && board[j] == board[6 + j]){
            if(board[j] == 1 || board[j] == 2){
                return board[j];
            }
        }
    }

    //check for diagonals
    if(board[0] == board[4] && board[0] == board[8]){
        if(board[0] == 1 || board[0] == 2){
            return board[0];
        }
    }
    if(board[2] == board[4] && board[2] == board[6]){
        if(board[2] == 1 || board[2] == 2){
            return board[2];
        }
    }

    return -1;
}

int signal_error(struct arguments* arg){

    int ret;
    char* for_client = "Something went wrong in the server";
    ret = send_message(arg, for_client, TXT);
    if(ret){
        return 1;
    }
    ret = send_message(arg, NULL, LFT);
    if(ret){
        return 1;
    }
    free(arg);
    return 0;
}