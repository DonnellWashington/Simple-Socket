#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<openssl/sha.h>

#define MAX 5
#define BUFFER 1024

// TRANFER THE FILE BETWEEEN 3 DIFFEENT COMPUTERS
// THE FIRST ARG IS THE SERVER IP AND THE REMAINING IPS ARE CLIENTS
// THE LAST TWO ARGS ARE THE FILENAME AND THE PORT NUMBER


// START THE SERVER
void run_server(int port, char *filename, int final){

    // SOCKET DESCRIPTOR AND NEW SOCKETS/ADDRESSES
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);


    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1){
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    
    // IP VERSION, ADDRESS, AND PORT NUMBER
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // BIND SOCKET TO IP AND PORT NUMBER 
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0){
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    if(listen(server_fd, MAX) < 0){
        perror("Listne failed");
        exit(EXIT_FAILURE);
    }


    printf("Listening on port %d...\n", port);
    

    // ACCEPT CLINET CONNECTIONS
    new_socket = accept(server_fd, (struct sockaddr *) &address, &addrlen);

    if (new_socket < 0){
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    
    printf("client conneted!\n");

    // CLOSE SOCKETS AFTER COMMUNCATION
    close(new_socket);
    close(server_fd);

}

// STARTING THE CLIENT
void run_client(char *server_ip, int port, char *filename, int reversing){



}

// CREATING THE HAS FOR THE FILE
void hashit(const char *filename, unsigned char *hash);

// PRINT THE HASH
void printHash(unsigned char *hash){

    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++){
        printf("%02x", hash[i]);
    }

    printf("\n");

}

// REVERSE THE FILE
void reverse(const char *filename);

int main(int argc, char *argv[]){


    // CHECK FOR THE CORRECT NUMBER OF COMMAND LINE ARGS IF NOT ENOUGH EXIT THE PROGRAM
    if (argc != 6){
        fprintf(stderr, "Usage: %s <role> <server_ip/A> <client1_ip/B> <client2_ip/C> <filename> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // STORING ARGS
    char *role = argv[1];
    char *server_ip = argv[1];
    char *filename = argv[4];
    int port = atoi(argv[5]);

    if (strcmp(role, "A") == 0){
        printf("Computer A (Sender and final receiver)\n");
        run_client(server_ip, port, filename, 0);
        run_server(port, filename, 0);
    }
    else if (strcmp(role, "B") == 0){
        printf("B middle computer\n");
        run_server(port, filename, 0);
        reverse(filename);
        run_client(server_ip, port, filename, 1);
    }
    else if (strcmp(role, "C" == 0))
    {
        printf("Last computer C\n");
        run_server(port, filename, 0);
        reverse(filename);
        run_client(server_ip, port, filename, 0);
    }
    else{
        printf("Use A, B, or C\n");
        return 1;
    }
    

    return 0;

}