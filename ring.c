#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<string.h>
#include<stdbool.h>
#include<stdio.h>

void dieWithUserMessage(const char *msg, const char *detail);             // HANDLING ERROR WITH USER MESSAGE

void DieWithSystemMessage(const char *msg, const char * detail);            // MIGHT NEED TO HANDLING ERROR WITH SYS MESSAGE

void PrintSocketAddress(const struct sockAddr *address, FILE *stream);      //WE PROB WONT NEED THIS

bool SockAddrsEqual(const struct sockaddr *addr1, const st);                //PROB WONT NEED THIS EITHER

int setupTCPServerSocket(const char *service);                              // CREATING THE SERVER SOCKET PORT NUMBER AS PARAMETER

int accpetTCPConnection(int clntSock);

int setupTCPClientSocket(const char *server, const char *service);