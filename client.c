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
#include <signal.h>

#include "common.h"
#include "client.h"


int main(int argc, char* argv[]) {
    
    if (argc != 3) {
        printf("Invalid number of arguments. Usage : ./client <ip_address> <port_number>\n");
        return 1; 
    }
    
    int ret;
    
    //get the ip address and the port number from the arguments passed to the client
    char* ip_address = argv[1];
    char* port = argv[2];

    //create the destination address
    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    inet_pton(AF_INET, ip_address, &(ad.sin_addr));
    ad.sin_port = htons(atoi(port));

    //create the socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd <0){
        perror("Error when creating the socket");
        return 1;
    }

    //create the arguments passed to the send_message and receive_message functions
    struct arguments* arg = malloc(sizeof(struct arguments));
    if(arg == NULL){
        perror("Error in allocating memory");
        return 1;
    }
    arg->sockfd = sockfd;
    arg->ad = ad;

    //send the first message to the server
    char* msg = "Hello";
    ret = send_message(arg, msg, TXT);
    if(ret == 1){
        return 1;
    }

    //now receive messages
    
    msg = NULL; //by default, msg is null

    int player_number = -1; //we need to player number for color display (green = our moves, red = opponent)
    
    while(1){ //as long as the game is going

        //wait for messages from server
        msg = receive_message(arg);
        if(msg == NULL){ //leave the game
            perror("Error in receiving the data");
            ret = send_message(arg, NULL, LFT);
            return 1;
        }

        //now we have a message

        if(msg[0]==END){ //END : the game is over or too many players

            if((unsigned char) msg[1] == 0xFF){
                printf("Too many players are playing !\n");
            }else if (msg[1] == 0) {
                printf("GAME OVER : draw\n");
            }else{
                printf("GAME OVER : ");
                if(player_number == msg[1]){
                    printf(GREEN "you won !\n" RESET);
                }else{
                    printf(RED "you lost :(\n" RESET);
                }
            }
            return 1;
        }
        
        else if(msg[0] == LFT){ //LFT : we must leave the game (either because the player left the game or something went wrong)
            return 1;
        }
        
        else if(msg[0] == MYM){ //MYM : check if we are asked to make a move 
            
            int col, row;
            
            while(1){
            
                printf("Make your move (format : row column) or press q to quit:");
                
                //store the input
                char buff[50];
                fgets(buff, 50, stdin);

                if(strcasecmp(buff, "q\n") == 0){ //the user wants to quit the game
                    ret = send_message(arg, NULL, LFT);
                    printf("Bye!\n"); 
                    return 1;
                }
                
                ret = sscanf(buff, "%d %d", &row, &col);
                if (ret != 2) { //check if the input is correct
                    printf("Invalid input format. Please enter row and column separated by a space.\n");
                    continue;
                }

                if(col > 2 || col < 0){ //check if the col is valid
                    
                    printf("This position isn't valid. Column must be : 0 or 1 or 2\n");
                    continue; //wait again for a response
                }

                else if(row > 2 || row < 0){ //check if the row is valid
                    
                    printf("This position isn't valid. Row must be : 0 or 1 or 2\n");
                    continue; //wait again for a response
                }

                break;

            }

            //compose the MOV message
            char msg[2];
            msg[0] = col;
            msg[1] = row;

            //send the reply to the server
            ret = send_message(arg, msg, MOV);
            if(ret == 1){
                return 1;
            }
                
        }

        else if(msg[0] == TXT){ //TXT : check if we have to display a message
            
            if(strcasecmp(msg+1, "game starts")==0){
                printf(GREEN"%s\n"RESET, msg+1);
            }else if(strcasecmp(msg+1, "the other player left") == 0){
                printf(RED"%s\n"RESET, msg+1);
            }
            else{
                printf("%s\n", msg+1);
            }
        }
        
        else if (msg[0] == FYI){ //FYI : check if we have to show the board
            
            int n = msg[1];
            
            //let us find out with what player numberare we
            if(player_number < 0){ //if we don't know (so we establish it only once)
                if(n==0){ //if no one played already, we are player 1 = o
                    player_number = 1;
                }else{
                    player_number = 2; //otherwise, we are player 2 = x
                }
            }

            char* start = msg+2;
            int i, j, p;
            int col, row, player;
            int board[9]= {0};

            if(n>0){ //if there are positions of the board
                //get the board information
                for(i = 0; i<n*3; i+=3){
                    player = start[i];
                    col = start[i+1];
                    row = start[i+2];
                    p = row*3 + col;
                    board[p] = player;
                }
            }

            //now we print the board
            printf("+---------+---------+---------+\n");
            for(i=0; i< 3; i++){//for each row
                printf("| ");
                for(j=0; j< 3; j++){//for each col
                    p = i*3 + j;
                    if(board[p]==1){
                        if(player_number == 1){
                            printf(GREEN"   o   "RESET);
                        }
                        else{
                            printf(RED "   o   "RESET);
                        }
                        printf(" | ");
                    }
                    else if(board[p]==2){
                        if(player_number == 2){
                            printf(GREEN"   x   "RESET);
                        }
                        else{
                            printf(RED "   x   "RESET);
                        }
                        printf(" | ");
                    }
                    else{
                        printf(" (%d,%d) ", i, j);
                        printf(" | ");
                    }
                }
                printf("\n");//we did one row
                printf("+---------+---------+---------+\n");

            }

        }

        free(msg); //free the memory allocated at msg by receive message
        
        msg = NULL; //reset msg to null

    }

    //close the socket
    close(sockfd);

    //free the memory allocated to the functions argument
    free(arg);

    return 0;
}

char* receive_message(struct arguments* arg){

    char* buffer = (char*)malloc(SIZE * sizeof(char));
    if (buffer == NULL) {
        perror("Error when allocating memory");
        return NULL;
    }

    socklen_t addrlen = sizeof(arg->ad);
    int len = recvfrom(arg->sockfd, buffer, SIZE - 1, 0, (struct sockaddr*)&(arg->ad), &addrlen);

    if (len < 0){
        perror("Error when receiving the data");
        free(buffer); //free buffer
        return NULL;
    }

    buffer[len] = '\0';

    return buffer;

}
