#ifndef COMMON_H
#define COMMON_H

//define the encoding of the message headers
#define FYI 0x01
#define MYM 0x02
#define END 0x03 
#define TXT 0x04
#define MOV 0x05
#define LFT 0x06

//define the max size of a tranported messages
#define SIZE 100

//define colors for color dispaly in the terminal
#define RESET   "\x1B[0m"
#define YELLOW  "\x1B[33m"
#define CYAN    "\x1B[36m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"


/*
holds information needed for sending data
- sockfd : the socket number
- ad : the destination address
*/
struct arguments
{
	int sockfd;
	struct sockaddr_in ad;
};


/*
@brief : sends a message in the appropriate format

@param arg : pointer to a struct arguments holding the transport informations
@param msg : message to be sent (char*)
@param type : header of the message

@return : 0 = success, 1 = failure
*/
int send_message(struct arguments* arg, char* msg, int type);


/*
@brief : for debugging prints a message byte by byte

@param msg : message (with the header) to be printed
*/
void print_message(char* msg);


#endif
