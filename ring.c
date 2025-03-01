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
void run_server(int port, char *filename) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);
    new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (new_socket < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    // Receive hash
    unsigned char received_hash[SHA512_DIGEST_LENGTH];
    recv(new_socket, received_hash, SHA512_DIGEST_LENGTH, 0);

    // Receive file
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("File open failed");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER];
    int bytesRead;
    while ((bytesRead = recv(new_socket, buffer, BUFFER, 0)) > 0) {
        fwrite(buffer, 1, bytesRead, file);
    }

    fclose(file);
    close(new_socket);
    close(server_fd);

    // Compute hash
    unsigned char computed_hash[SHA512_DIGEST_LENGTH];
    hash_file(filename, computed_hash);

    printf("Received file hash: ");
    print_hash(received_hash);
    printf("Computed file hash: ");
    print_hash(computed_hash);

    if (memcmp(received_hash, computed_hash, SHA512_DIGEST_LENGTH) == 0) {
        printf("File received successfully.\n");
    } else {
        printf("Hash mismatch! File may be corrupted.\n");
        exit(EXIT_FAILURE);
    }

    // Print file content
    file = fopen(filename, "rb");
    while (fgets(buffer, BUFFER, file)) {
        printf("%s", buffer);
    }
    fclose(file);
}


// STARTING THE CLIENT
void run_client(char *server_ip, int port, char *filename) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;

    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Compute hash
    unsigned char hash[SHA512_DIGEST_LENGTH];
    hash_file(filename, hash);

    // Send hash
    send(sockfd, hash, SHA512_DIGEST_LENGTH, 0);

    // Send file
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("File open failed");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER];
    int bytesRead;
    while ((bytesRead = fread(buffer, 1, BUFFER, file)) > 0) {
        send(sockfd, buffer, bytesRead, 0);
    }

    fclose(file);
    close(sockfd);
}

// CREATING THE HAS FOR THE FILE
void hash_file(const char *filename, unsigned char *hash) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("File open failed");
        exit(EXIT_FAILURE);
    }

    SHA512_CTX sha_ctx;
    SHA512_Init(&sha_ctx);

    unsigned char buffer[BUFFER];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        SHA512_Update(&sha_ctx, buffer, bytesRead);
    }

    fclose(file);
    SHA512_Final(hash, &sha_ctx);
}

// PRINT THE HASH
void print_hash(unsigned char *hash) {
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}


// REVERSE THE FILE
void reverse_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("File open failed");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(size + 1);
    fread(content, 1, size, file);
    fclose(file);

    file = fopen(filename, "wb");
    for (long i = size - 1; i >= 0; i--) {
        fputc(content[i], file);
    }

    fclose(file);
    free(content);
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <role> <server_ip> <client1_ip> <client2_ip> <filename> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *role = argv[1];
    char *server_ip = argv[2];
    char *client1_ip = argv[3];
    char *client2_ip = argv[4];
    char *filename = argv[5];
    int port = atoi(argv[6]);

    if (strcmp(role, "A") == 0) {
        printf("Computer A: Sending file and final verification...\n");
        run_client(server_ip, port, filename);
        run_server(port, filename);
    } else if (strcmp(role, "B") == 0) {
        printf("Computer B: Intermediate node...\n");
        run_server(port, filename);
        reverse_file(filename);
        run_client(client2_ip, port, filename);
    } else if (strcmp(role, "C") == 0) {
        printf("Computer C: Final transformation...\n");
        run_server(port, filename);
        reverse_file(filename);
        run_client(client1_ip, port, filename);
    } else {
        fprintf(stderr, "Invalid role. Use A, B, or C.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}