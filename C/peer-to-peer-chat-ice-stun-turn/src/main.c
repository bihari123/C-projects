#include "../include/common.h"
#include "../include/socket.h"
#include "../include/ice.h"

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    int is_server = 0;
    int port = DEFAULT_PORT;
    int use_ice = 1;
    char peer_ip[16] = "127.0.0.1";
    char stun_server[64] = STUN_SERVER;
    int stun_port = STUN_PORT;
    char turn_server[64] = TURN_SERVER;
    int turn_port = TURN_PORT;
    char turn_user[32] = TURN_USER;
    char turn_pass[32] = TURN_PASS;
    
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
        } else if (strcmp(argv[i], "--no-ice") == 0) {
            use_ice = 0;
        } else if (strcmp(argv[i], "--stun") == 0 && i + 1 < argc) {
            strncpy(stun_server, argv[i + 1], 63);
            stun_server[63] = '\0';
            i++;
        } else if (strcmp(argv[i], "--stun-port") == 0 && i + 1 < argc) {
            stun_port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--turn") == 0 && i + 1 < argc) {
            strncpy(turn_server, argv[i + 1], 63);
            turn_server[63] = '\0';
            i++;
        } else if (strcmp(argv[i], "--turn-port") == 0 && i + 1 < argc) {
            turn_port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--turn-user") == 0 && i + 1 < argc) {
            strncpy(turn_user, argv[i + 1], 31);
            turn_user[31] = '\0';
            i++;
        } else if (strcmp(argv[i], "--turn-pass") == 0 && i + 1 < argc) {
            strncpy(turn_pass, argv[i + 1], 31);
            turn_pass[31] = '\0';
            i++;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -s                 Run as server\n");
            printf("  -p <port>          Specify port (default: %d)\n", DEFAULT_PORT);
            printf("  -c <ip>            Connect to specified IP (client mode)\n");
            printf("  --no-ice           Disable ICE (use direct socket connection)\n");
            printf("  --stun <server>    Specify STUN server (default: %s)\n", STUN_SERVER);
            printf("  --stun-port <port> Specify STUN port (default: %d)\n", STUN_PORT);
            printf("  --turn <server>    Specify TURN server\n");
            printf("  --turn-port <port> Specify TURN port (default: %d)\n", TURN_PORT);
            printf("  --turn-user <user> Specify TURN username\n");
            printf("  --turn-pass <pass> Specify TURN password\n");
            printf("  --help             Show this help message\n");
            exit(EXIT_SUCCESS);
        }
    }
    
    peer_t peer;
    memset(&peer, 0, sizeof(peer_t));
    global_peer = &peer;
    
    // Setup mutexes and conditions
    g_mutex_init(&sdp_mutex);
    g_cond_init(&sdp_cond);
    
    if (use_ice) {
        printf("Using ICE for NAT traversal\n");
        printf("STUN server: %s:%d\n", stun_server, stun_port);
        if (strlen(turn_server) > 0) {
            printf("TURN server: %s:%d\n", turn_server, turn_port);
            if (strlen(turn_user) > 0) {
                printf("TURN credentials: %s:****\n", turn_user);
            }
        }
        
        // Initialize ICE
        if (!setup_ice(&peer, is_server)) {
            fprintf(stderr, "Failed to set up ICE. Falling back to direct connection.\n");
            use_ice = 0;
        }
    }
    
    if (!use_ice) {
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
    }
    
    // First, handle SDP exchange if using ICE
    if (use_ice) {
        // Wait for local SDP to be generated
        while (peer.local_sdp == NULL) {
            usleep(100000);  // Sleep for 100ms
        }
        
        // Get the remote SDP
        printf("Please paste the remote peer's SDP below and press Enter twice when done:\n");
        fflush(stdout);
        
        char remote_sdp_buffer[4096] = {0};
        char line[1024];
        int line_count = 0;
        
        while (fgets(line, sizeof(line), stdin)) {
            // Check for an empty line (just newline) to signal the end of input
            if (line[0] == '\n' && line_count > 0) {
                break;
            }
            
            strncat(remote_sdp_buffer, line, sizeof(remote_sdp_buffer) - strlen(remote_sdp_buffer) - 1);
            line_count++;
        }
        
        if (line_count > 0) {
            printf("Processing remote SDP...\n");
            fflush(stdout);
            
            if (!process_remote_sdp(&peer, remote_sdp_buffer)) {
                fprintf(stderr, "Failed to process remote SDP\n");
            }
            
            // Signal that we have processed the remote SDP
            g_mutex_lock(&sdp_mutex);
            got_remote_sdp = TRUE;
            g_mutex_unlock(&sdp_mutex);
        }
    }
    
    // Wait for ICE connection to be established
    if (use_ice) {
        printf("Waiting for ICE connection to be established...\n");
        fflush(stdout);
        
        // Wait for up to 30 seconds for ICE to connect
        int timeout = 30;
        while (peer.ice_state != ICE_CONNECTED && peer.ice_state != ICE_FAILED && timeout > 0) {
            sleep(1);
            timeout--;
            
            if (timeout % 5 == 0) {
                printf("Still waiting for ICE connection... %d seconds left\n", timeout);
                fflush(stdout);
            }
        }
        
        if (peer.ice_state != ICE_CONNECTED) {
            fprintf(stderr, "ICE connection failed or timed out\n");
            if (strlen(turn_server) == 0) {
                fprintf(stderr, "Try using a TURN server with --turn, --turn-user, and --turn-pass options\n");
            }
        }
    }
    
    // Main loop for sending messages
    char buffer[BUFFER_SIZE];
    printf("Start typing messages (press Ctrl+C to exit):\n");
    fflush(stdout);
    
    while (1) {
        // Read input from user
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break;  // Handle EOF
        }
        
        if (use_ice && peer.ice_state == ICE_CONNECTED) {
            // Send using ICE
            if (!send_ice_message(&peer, buffer, strlen(buffer))) {
                fprintf(stderr, "Failed to send message over ICE\n");
            }
        } else if (!use_ice) {
            // Send using direct socket
            if (send(peer.socket, buffer, strlen(buffer), 0) < 0) {
                perror("Failed to send message over socket");
            }
        } else {
            printf("Cannot send message - no connection established\n");
            fflush(stdout);
        }
    }
    
    // Clean up
    if (use_ice) {
        cleanup_ice(&peer);
    } else {
        close(peer.socket);
    }
    
    g_mutex_clear(&sdp_mutex);
    g_cond_clear(&sdp_cond);
    
    return 0;
}