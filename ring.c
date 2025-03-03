#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/sha.h>

#define MAX_BUF 1024
#define ACK1 "ACK1"
#define ACK2 "ACK2"

//--------------------------------------------------
// Error handling functions
void DieWithUserMessage(const char *msg, const char *detail) {
    fprintf(stderr, "%s: %s\n", msg, detail);
    exit(1);
}

void DieWithSystemMessage(const char *msg) {
    perror(msg);
    exit(1);
}

//--------------------------------------------------
// Compute SHA512 hash of a file (64 bytes)
void compute_sha512(const char *filename, unsigned char *hash_output) {
    FILE *file = fopen(filename, "rb");
    if (!file)
        DieWithUserMessage("fopen() failed", "unable to open file for hashing");

    SHA512_CTX sha_ctx;
    if (SHA512_Init(&sha_ctx) != 1)
        DieWithUserMessage("SHA512_Init failed", "");

    unsigned char buffer[MAX_BUF];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, MAX_BUF, file)) != 0) {
        SHA512_Update(&sha_ctx, buffer, bytesRead);
    }
    SHA512_Final(hash_output, &sha_ctx);
    fclose(file);
}

//--------------------------------------------------
// Reverse the contents of a file.
// Reads input from 'input_filename' and writes reversed data to 'output_filename'.
void reverse_file(const char *input_filename, const char *output_filename) {
    FILE *fin = fopen(input_filename, "rb");
    if (!fin)
        DieWithUserMessage("reverse_file: fopen() failed", "input file");

    fseek(fin, 0, SEEK_END);
    long size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    char *buffer = malloc(size);
    if (!buffer) {
        fclose(fin);
        DieWithUserMessage("reverse_file", "malloc failed");
    }
    if (fread(buffer, 1, size, fin) != (size_t)size) {
        free(buffer);
        fclose(fin);
        DieWithUserMessage("reverse_file", "fread failed");
    }
    fclose(fin);

    FILE *fout = fopen(output_filename, "wb");
    if (!fout) {
        free(buffer);
        DieWithUserMessage("reverse_file: fopen() failed", "output file");
    }
    for (long i = size - 1; i >= 0; i--) {
        fputc(buffer[i], fout);
    }
    fclose(fout);
    free(buffer);
    printf("Reversed file '%s' -> '%s'.\n", input_filename, output_filename);
}

//--------------------------------------------------
// Helper: Print the contents of a file to the screen.
void print_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file)
        DieWithUserMessage("print_file", "fopen failed");
    int ch;
    while ((ch = fgetc(file)) != EOF)
        putchar(ch);
    fclose(file);
}

//--------------------------------------------------
// Client (sender) function using handshake protocol.
// It sends: [SHA512 hash][file name length & file name],
// waits for ACK1, then sends [file size][file content],
// and finally waits for ACK2.
void client_send(const char *dest_ip, int port, const char *filename) {
    int sock;
    struct sockaddr_in dest_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        DieWithSystemMessage("socket() failed");
    
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr(dest_ip);
    
    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
        DieWithSystemMessage("connect() failed");
    
    // Phase 1: Send SHA512 hash (64 bytes)
    unsigned char hash[SHA512_DIGEST_LENGTH];
    compute_sha512(filename, hash);
    if (send(sock, hash, SHA512_DIGEST_LENGTH, 0) != SHA512_DIGEST_LENGTH)
        DieWithSystemMessage("send() hash failed");
    printf("Client: Sent SHA512 hash.\n");
    
    // Phase 2: Send file name length and file name
    uint32_t name_len = strlen(filename);
    uint32_t net_name_len = htonl(name_len);
    if (send(sock, &net_name_len, sizeof(net_name_len), 0) != sizeof(net_name_len))
        DieWithSystemMessage("send() file name length failed");
    if (send(sock, filename, name_len, 0) != name_len)
        DieWithSystemMessage("send() file name failed");
    printf("Client: Sent file name '%s'.\n", filename);
    
    // Wait for ACK1 from receiver
    char ack_buf[16] = {0};
    if (recv(sock, ack_buf, sizeof(ack_buf)-1, 0) <= 0)
        DieWithSystemMessage("recv() ACK1 failed");
    if (strcmp(ack_buf, ACK1) != 0)
        DieWithUserMessage("Client", "Did not receive proper ACK1");
    printf("Client: Received ACK1.\n");
    
    // Phase 3: Send file size and file content
    FILE *file = fopen(filename, "rb");
    if (!file)
        DieWithUserMessage("fopen() failed", "unable to open file for sending");
    fseek(file, 0, SEEK_END);
    uint32_t file_size = ftell(file);
    rewind(file);
    uint32_t net_file_size = htonl(file_size);
    if (send(sock, &net_file_size, sizeof(net_file_size), 0) != sizeof(net_file_size))
        DieWithSystemMessage("send() file size failed");
    printf("Client: Sent file size: %u bytes.\n", file_size);
    
    char buffer[MAX_BUF];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, MAX_BUF, file)) > 0) {
        if (send(sock, buffer, bytesRead, 0) != bytesRead)
            DieWithSystemMessage("send() file content failed");
    }
    fclose(file);
    printf("Client: Sent file contents.\n");
    
    // Wait for ACK2 from receiver
    memset(ack_buf, 0, sizeof(ack_buf));
    if (recv(sock, ack_buf, sizeof(ack_buf)-1, 0) <= 0)
        DieWithSystemMessage("recv() ACK2 failed");
    if (strcmp(ack_buf, ACK2) != 0)
        DieWithUserMessage("Client", "Did not receive proper ACK2");
    printf("Client: Received ACK2. Transfer complete.\n");
    
    close(sock);
}

//--------------------------------------------------
// Server (receiver) function using handshake protocol.
// This version is used by B and C when receiving a file (from A or B).
// The file is saved as "received_file.txt".
void server_receive(const char *bind_ip, int port) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    
    if ((serv_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        DieWithSystemMessage("socket() failed");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(bind_ip);
    
    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        DieWithSystemMessage("bind() failed");
    
    if (listen(serv_sock, 5) < 0)
        DieWithSystemMessage("listen() failed");
    
    printf("Server (%s): Listening on %s:%d...\n", bind_ip, bind_ip, port);
    
    if ((clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_len)) < 0)
        DieWithSystemMessage("accept() failed");
    printf("Server: Connection accepted.\n");
    
    // Receive SHA512 hash (64 bytes)
    unsigned char hash[SHA512_DIGEST_LENGTH];
    if (recv(clnt_sock, hash, SHA512_DIGEST_LENGTH, 0) != SHA512_DIGEST_LENGTH)
        DieWithSystemMessage("recv() hash failed");
    printf("Server: Received SHA512 hash.\n");
    
    // Receive file name length and file name
    uint32_t net_name_len;
    if (recv(clnt_sock, &net_name_len, sizeof(net_name_len), 0) != sizeof(net_name_len))
        DieWithSystemMessage("recv() file name length failed");
    uint32_t name_len = ntohl(net_name_len);
    char *filename = malloc(name_len + 1);
    if (!filename)
        DieWithUserMessage("malloc() failed", "for file name");
    if (recv(clnt_sock, filename, name_len, 0) != name_len)
        DieWithSystemMessage("recv() file name failed");
    filename[name_len] = '\0';
    printf("Server: Received file name: '%s'\n", filename);
    
    // Send ACK1 to sender
    if (send(clnt_sock, ACK1, strlen(ACK1), 0) != (ssize_t)strlen(ACK1))
        DieWithSystemMessage("send() ACK1 failed");
    printf("Server: Sent ACK1.\n");
    
    // Receive file size and file content
    uint32_t net_file_size;
    if (recv(clnt_sock, &net_file_size, sizeof(net_file_size), 0) != sizeof(net_file_size))
        DieWithSystemMessage("recv() file size failed");
    uint32_t file_size = ntohl(net_file_size);
    printf("Server: Expecting file of size %u bytes.\n", file_size);
    
    FILE *file = fopen("received_file.txt", "wb");
    if (!file)
        DieWithUserMessage("fopen() failed", "for writing received file");
    
    uint32_t total_received = 0;
    char buffer[MAX_BUF];
    ssize_t bytesReceived;
    while (total_received < file_size) {
        bytesReceived = recv(clnt_sock, buffer, MAX_BUF, 0);
        if (bytesReceived <= 0)
            DieWithSystemMessage("recv() file content failed");
        fwrite(buffer, 1, bytesReceived, file);
        total_received += bytesReceived;
    }
    fclose(file);
    printf("Server: Received file contents.\n");
    
    // Send ACK2 to sender
    if (send(clnt_sock, ACK2, strlen(ACK2), 0) != (ssize_t)strlen(ACK2))
        DieWithSystemMessage("send() ACK2 failed");
    printf("Server: Sent ACK2.\n");
    
    free(filename);
    close(clnt_sock);
    close(serv_sock);
}

//--------------------------------------------------
// Server (receiver) function with hash verification.
// This is used by Computer A (Role A) when receiving the file from C.
// It performs the handshake, then computes the file's hash and compares it to the
// hash sent by the sender. If they match, it sends a final acknowledgment and prints the file;
// otherwise, it sends an error and exits.
void server_receive_verify(const char *bind_ip, int port) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    
    if ((serv_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        DieWithSystemMessage("socket() failed");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(bind_ip);
    
    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        DieWithSystemMessage("bind() failed");
    
    if (listen(serv_sock, 5) < 0)
        DieWithSystemMessage("listen() failed");
    
    printf("Server (Role A verify): Listening on %s:%d...\n", bind_ip, port);
    
    if ((clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_len)) < 0)
        DieWithSystemMessage("accept() failed");
    printf("Server: Connection accepted.\n");
    
    // Receive expected SHA512 hash (64 bytes)
    unsigned char expected_hash[SHA512_DIGEST_LENGTH];
    if (recv(clnt_sock, expected_hash, SHA512_DIGEST_LENGTH, 0) != SHA512_DIGEST_LENGTH)
        DieWithSystemMessage("recv() expected hash failed");
    printf("Server: Received expected SHA512 hash.\n");
    
    // Receive file name length and file name
    uint32_t net_name_len;
    if (recv(clnt_sock, &net_name_len, sizeof(net_name_len), 0) != sizeof(net_name_len))
        DieWithSystemMessage("recv() file name length failed");
    uint32_t name_len = ntohl(net_name_len);
    char *filename = malloc(name_len + 1);
    if (!filename)
        DieWithUserMessage("malloc() failed", "for file name");
    if (recv(clnt_sock, filename, name_len, 0) != name_len)
        DieWithSystemMessage("recv() file name failed");
    filename[name_len] = '\0';
    printf("Server: Received file name: '%s'\n", filename);
    
    // Send ACK1 to sender
    if (send(clnt_sock, ACK1, strlen(ACK1), 0) != (ssize_t)strlen(ACK1))
        DieWithSystemMessage("send() ACK1 failed");
    printf("Server: Sent ACK1.\n");
    
    // Receive file size and file content
    uint32_t net_file_size;
    if (recv(clnt_sock, &net_file_size, sizeof(net_file_size), 0) != sizeof(net_file_size))
        DieWithSystemMessage("recv() file size failed");
    uint32_t file_size = ntohl(net_file_size);
    printf("Server: Expecting file of size %u bytes.\n", file_size);
    
    FILE *file = fopen("received_file.txt", "wb");
    if (!file)
        DieWithUserMessage("fopen() failed", "for writing received file");
    
    uint32_t total_received = 0;
    char buffer[MAX_BUF];
    ssize_t bytesReceived;
    while (total_received < file_size) {
        bytesReceived = recv(clnt_sock, buffer, MAX_BUF, 0);
        if (bytesReceived <= 0)
            DieWithSystemMessage("recv() file content failed");
        fwrite(buffer, 1, bytesReceived, file);
        total_received += bytesReceived;
    }
    fclose(file);
    printf("Server: Received file contents.\n");
    
    // Send ACK2 to sender
    if (send(clnt_sock, ACK2, strlen(ACK2), 0) != (ssize_t)strlen(ACK2))
        DieWithSystemMessage("send() ACK2 failed");
    printf("Server: Sent ACK2.\n");
    
    // Now compute SHA512 hash on "received_file.txt" and compare with expected_hash
    unsigned char computed_hash[SHA512_DIGEST_LENGTH];
    compute_sha512("received_file.txt", computed_hash);
    
    if (memcmp(expected_hash, computed_hash, SHA512_DIGEST_LENGTH) == 0) {
        printf("Hashes match. File is intact.\n");
        // Send final ack to sender
        if (send(clnt_sock, "FINAL_ACK", strlen("FINAL_ACK"), 0) != (ssize_t)strlen("FINAL_ACK"))
            DieWithSystemMessage("send() final ack failed");
        // Print file contents to screen
        printf("Final file contents received:\n");
        print_file("received_file.txt");
    } else {
        printf("Error: Hashes do not match!\n");
        if (send(clnt_sock, "ERR", strlen("ERR"), 0) != (ssize_t)strlen("ERR"))
            DieWithSystemMessage("send() error message failed");
        exit(1);
    }
    
    free(filename);
    close(clnt_sock);
    close(serv_sock);
}

//--------------------------------------------------
// Main function – Chain the transfer among three computers.
// Roles and behavior:
//   Role A (Computer A):
//     • Phase 1: Act as client; send file (hash, filename, contents) to B.
//     • Phase 2: Switch to server mode with verification to receive file from C.
//   Role B (Computer B):
//     • Immediately act as server to receive file from A.
//     • Print the received file to the screen.
//     • Reverse the file and then act as client to send the reversed file to C.
//   Role C (Computer C):
//     • Immediately act as server to receive file from B.
//     • Print the received file to the screen.
//     • Reverse the file and then act as client to send it back to A.
int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "Role A: %s A <B_IP> <file> <port>\n", argv[0]);
        fprintf(stderr, "Role B: %s B <C_IP> <port>\n", argv[0]);
        fprintf(stderr, "Role C: %s C <A_IP> <port>\n", argv[0]);
        exit(1);
    }
    
    char role = argv[1][0];
    int port = atoi(argv[argc-1]);
    
    if (role == 'A') {
        // Role A: Send file to B, then switch to server (with verification) to receive file from C.
        if (argc != 5)
            DieWithUserMessage("Usage", "A <B_IP> <file> <port>");
        char *B_IP = argv[2];
        char *file_to_send = argv[3];
        printf("Role A: Sending file to B (%s)...\n", B_IP);
        client_send(B_IP, port, file_to_send);
        printf("Role A: Now switching to server mode to wait for file from C...\n");
        server_receive_verify("0.0.0.0", port);
    } else if (role == 'B') {
        // Role B: Start as server to receive file from A.
        if (argc != 4)
            DieWithUserMessage("Usage", "B <C_IP> <port>");
        char *C_IP = argv[2];
        printf("Role B: Starting as server, waiting for file from A...\n");
        server_receive("0.0.0.0", port);
        printf("Role B: File contents received from A:\n");
        print_file("received_file.txt");
        printf("\nRole B: Reversing received file...\n");
        reverse_file("received_file.txt", "reversed_file.txt");
        printf("Role B: Now sending reversed file to C (%s)...\n", C_IP);
        client_send(C_IP, port, "reversed_file.txt");
    } else if (role == 'C') {
        // Role C: Start as server to receive file from B.
        if (argc != 4)
            DieWithUserMessage("Usage", "C <A_IP> <port>");
        char *A_IP = argv[2];
        printf("Role C: Starting as server, waiting for file from B...\n");
        server_receive("0.0.0.0", port);
        printf("Role C: File contents received from B:\n");
        print_file("received_file.txt");
        printf("\nRole C: Reversing received file...\n");
        reverse_file("received_file.txt", "reversed_file.txt");
        printf("Role C: Now sending reversed file back to A (%s)...\n", A_IP);
        client_send(A_IP, port, "reversed_file.txt");
    } else {
        DieWithUserMessage("Invalid role", "Choose A, B, or C");
    }
    
    return 0;
}
