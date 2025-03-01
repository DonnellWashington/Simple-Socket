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

    server_fd = socket(AF_INET, SOCK_STREAM);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 1);
    new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    unsigned char received_hash[SHA224_DIGEST_LENGTH];
    recv(new_socket, received_hash, SHA224_DIGEST_LENGTH, 0);

    FILE *file = fopen(filename, "wb");
    char buffer[BUFFER];
    int bytesRead;

    while((bytesRead = recv(new_socket,buffer, BUFFER, 0)) > 0){
        fwrite(buffer, 1, bytesRead, file);
    }

    fclose(file);
    close(new_socket);
    close(server_fd);

    unsigned char computed_hash[SHA512_DIGEST_LENGTH];
    hashit(filename, computed_hash);

}

// STARTING THE CLIENT
void run_client(char *server_ip, int port, char *filename, int reversing){

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));

    unsigned char hash[SHA512_DIGEST_LENGTH];
    hashit(filename, hash);


    FILE *file = fopen(filename, "rb");
    char buffer[BUFFER];

    send(sockfd, hash, SHA224_DIGEST_LENGTH, 0);

    while (fread(buffer, 1, BUFFER, file) > 0){
        send(sockfd, buffer, BUFFER, 0);
    }

    fclose(file);
    close(sockfd);

}

// CREATING THE HAS FOR THE FILE
void hashit(const char *filename, unsigned char *hash){

    FILE *file = fopen(filename, "rb");

    if (!file){
        perror("File cant be opened");
        exit(EXIT_FAILURE);
    }

    SHA512_CTX sha_ctx;
    SHA512_Init (&sha_ctx);

    unsigned char buffer[1024];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0){
        SHA512_Update(&sha_ctx, buffer, bytesRead);
    }

    fclose(file);

    SHA512_Final(hash, &sha_ctx);
    

}

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