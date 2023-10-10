#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "common.h"

int send_message(struct arguments* arg, char* msg, int type){
    
    int ret, len;
    
    if(type == MYM || type == LFT){ //those messages don't have any arguments, just the header
        len = 1;
    }else if (type == FYI){
        len = msg[1]*3 + 2;
    }else if(type == TXT){
        len=strlen(msg)+2;
    }else if(type == MOV){
        len = 3;
    }else if(type == END){
        len = 2;
    }

    char buffer[len];

    //first create the message by adding the header
    if(type == TXT){ 
        buffer[0] = (char) type; //add the header at the start of the buffer
        strcpy(buffer + 1, msg);
        buffer[len - 1] = '\0'; //add a null terminator at the end of TXT
        msg = buffer;
    }
    else if(type == MOV){
        buffer[0] = MOV;
        buffer[1] = msg[0];
        buffer[2] = msg[1];
        msg = buffer;
    }
    else if(type == END){
        buffer[0] = END;
        buffer[1] = msg[0];
        msg = buffer;
    }
    else if (type != FYI){ //for MYM, LFT
        buffer[0] = type;
        msg = buffer;
    }
        
    //now send the message
    ret = sendto(arg->sockfd, (const char*)msg, len, 0, (struct sockaddr*)&(arg->ad), sizeof(arg->ad));
    if (ret == -1) {
        perror("Error in sending the data\n");
        return 1;
    }

    return 0;
    
}

void print_message(char* msg){
    
    int i = 0;
    if(msg[0] == TXT){
        printf(" | ");
        while (msg[i] != '\0'){ 
            
            printf("%c", msg[i]);
            printf(" | ");
            i++;
        }
        printf("\\0");
        printf("\n");
    }
    if (msg[0]==FYI){
        int n = msg[1];
        for(i=0; i<n*3+2; i++){
            if(i==1){
                printf("nb filled pos : %d", msg[i]);
            }

            else if(i > 1 && (i-2)%3 == 0){
                printf("player %d", msg[i]);
            }
            else if(i > 1 && (i-1)%3 == 0){
                printf("row %d", msg[i]);
            }
            else if(i > 1 && i%3 == 0){
                printf("col %d", msg[i]);
            }
            else{
                printf("%d", msg[i]);
            }
            printf(" | ");
        }
        printf("\n");
    }


    if(msg[0] == MOV){
        for (i = 0; i < 3; i++) {
            printf(" %d |", msg[i]);
        }
        printf("\n");
    }

    if(msg[0] == END){
        for (i = 0; i < 2; i++) {
            printf(" %d |", msg[i]);
        }
        printf("\n");
    }

}
