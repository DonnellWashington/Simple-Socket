#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>

#define MAX 5

// TRANFER THE FILE BETWEEEN 3 DIFFEENT COMPUTERS
// THE FIRST ARG IS THE SERVER IP AND THE REMAINING IPS ARE CLIENTS
// THE LAST TWO ARGS ARE THE FILENAME AND THE PORT NUMBER

int main(int argc, char *argv[]){


    // CHECK FOR THE CORRECT NUMBER OF COMMAND LINE ARGS IF NOT ENOUGH EXIT THE PROGRAM
    if (argc < 6){
        fprintf(stderr, "Usage: %s <server_ip/A> <client1_ip/B> <client2_ip/C> <filename> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // STORING ARGS
    char *server_ip = argv[1];
    char *client1_ip = argv[2];
    char *client2_ip = argv[3];
    char *filename = argv[4];
    int port = atoi(argv[5]);


    // SOCKET DESCRIPTOR AND NEW SOCKETS/ADDRESSES
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // IP VERSION, ADDRESS, AND PORT NUMBER
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet(server_ip);
    address.sin_port = htons(port);

    // BIND SOCKET TO IP AND PORT NUMBER 
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0){
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }


    


    return 0;

}