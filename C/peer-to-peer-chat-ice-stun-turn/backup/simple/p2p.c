#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define DEFAULT_PORT 8080

typedef struct {
    int socket;
    struct sockaddr_in address;
} peer_t;

void *receive_messages(void *arg) {
    peer_t *peer = (peer_t *)arg;
    char buffer[BUFFER_SIZE];
    int bytes_received;
    
    while (1) {
        // Clear the buffer
        memset(buffer, 0, BUFFER_SIZE);
        
        // Receive message
        bytes_received = recv(peer->socket, buffer, BUFFER_SIZE, 0);
        
        if (bytes_received <= 0) {
            printf("Connection closed by peer\n");
            break;
        }
        
        printf("Received: %s", buffer);
    }
    
    return NULL;
}

int create_server(int port) {
    int server_fd;
    struct sockaddr_in address;
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Setup server address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // Bind socket to the address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server started on port %d\n", port);
    return server_fd;
}

peer_t connect_to_peer(const char *ip, int port) {
    peer_t peer;
    
    // Create socket
    if ((peer.socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set up peer address
    peer.address.sin_family = AF_INET;
    peer.address.sin_port = htons(port);
    
    // Convert IP address to binary form
    if (inet_pton(AF_INET, ip, &peer.address.sin_addr) <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }
    
    // Connect to the peer
    if (connect(peer.socket, (struct sockaddr *)&peer.address, sizeof(peer.address)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to peer at %s:%d\n", ip, port);
    return peer;
}

int main(int argc, char *argv[]) {
    int is_server = 0;
    int port = DEFAULT_PORT;
    char peer_ip[16] = "127.0.0.1";
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            is_server = 1;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            strncpy(peer_ip, argv[i + 1], 15);
            peer_ip[15] = '\0';
            i++;
        }
    }
    
    peer_t peer;
    pthread_t receive_thread;
    
    if (is_server) {
        // Create server and wait for connection
        int server_fd = create_server(port);
        int addrlen = sizeof(struct sockaddr_in);
        
        printf("Waiting for connection...\n");
        
        // Accept connection
        peer.socket = accept(server_fd, (struct sockaddr *)&peer.address, (socklen_t*)&addrlen);
        if (peer.socket < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }
        
        // Get peer IP
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer.address.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Connection accepted from %s:%d\n", client_ip, ntohs(peer.address.sin_port));
        
        // Close the server socket as we only need one connection
        close(server_fd);
    } else {
        // Connect to a peer
        peer = connect_to_peer(peer_ip, port);
    }
    
    // Create thread for receiving messages
    if (pthread_create(&receive_thread, NULL, receive_messages, (void *)&peer) != 0) {
        perror("Failed to create thread");
        exit(EXIT_FAILURE);
    }
    
    // Main loop for sending messages
    char buffer[BUFFER_SIZE];
    printf("Start typing messages (press Ctrl+C to exit):\n");
    
    while (1) {
        // Read input from user
        fgets(buffer, BUFFER_SIZE, stdin);
        
        // Send message
        send(peer.socket, buffer, strlen(buffer), 0);
    }
    
    // Clean up
    close(peer.socket);
    return 0;
}