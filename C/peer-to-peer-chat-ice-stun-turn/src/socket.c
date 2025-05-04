#include "../include/socket.h"

/**
 * Thread function to receive messages from a peer
 */
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

/**
 * Create a server socket
 */
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

/**
 * Connect to a peer
 */
peer_t connect_to_peer(const char *ip, int port) {
    peer_t peer;
    memset(&peer, 0, sizeof(peer_t));
    
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