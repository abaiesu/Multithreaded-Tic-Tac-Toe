#ifndef CLIENT_H
#define CLIENT_H

/*
@brief : receives a message from the socket and address stored in arg

@param arg : pointer to a struct arguments holding the transport informations

@return : char* containing the received message (allocated in the heap)
*/
char* receive_message(struct arguments* arg);

#endif