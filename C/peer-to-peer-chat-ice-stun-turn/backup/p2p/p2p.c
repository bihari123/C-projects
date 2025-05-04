#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <glib.h>
#include <nice/agent.h>
#include <nice/nice.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define DEFAULT_PORT 8080
#define STUN_SERVER "stun.l.google.com"
#define STUN_PORT 19302
#define TURN_SERVER ""
#define TURN_PORT 3478
#define TURN_USER ""
#define TURN_PASS ""

// ICE connection states
typedef enum {
    ICE_GATHERING,
    ICE_CONNECTING,
    ICE_CONNECTED,
    ICE_FAILED
} ice_state_t;

typedef struct {
    int socket;
    struct sockaddr_in address;
    NiceAgent *agent;
    guint stream_id;
    guint component_id;
    GMainLoop *loop;
    GThread *thread;
    ice_state_t ice_state;
    gboolean controlling;
    gchar *local_sdp;
    gchar *remote_sdp;
} peer_t;

// Global variables
static GMutex sdp_mutex;
static GCond sdp_cond;
static gboolean got_remote_sdp = FALSE;
static peer_t *global_peer = NULL;

// Forward declarations
void *receive_messages(void *arg);
void *glib_thread_func(void *data);
gboolean get_ice_candidates(peer_t *peer);
gboolean process_remote_sdp(peer_t *peer, const gchar *sdp);
void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer user_data);
void cb_component_state_changed(NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer user_data);
void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer user_data);

void *glib_thread_func(void *data) {
    GMainLoop *loop = (GMainLoop *)data;
    g_main_loop_run(loop);
    return NULL;
}

// Function to handle STUN/TURN discovery and ICE negotiation
gboolean setup_ice(peer_t *peer, gboolean controlling) {
    GMainContext *context;
    
    // Create main context and loop for the ICE agent
    context = g_main_context_new();
    peer->loop = g_main_loop_new(context, FALSE);
    
    // Start the GLib main loop in a separate thread
    GError *error = NULL;
    peer->thread = g_thread_new("ice-thread", glib_thread_func, peer->loop);
    
    // Create the ICE agent
    peer->agent = nice_agent_new(context, NICE_COMPATIBILITY_RFC5245);
    if (!peer->agent) {
        g_warning("Failed to create ICE agent");
        return FALSE;
    }
    
    // Set the STUN server
    g_object_set(G_OBJECT(peer->agent), "stun-server", STUN_SERVER, NULL);
    g_object_set(G_OBJECT(peer->agent), "stun-server-port", STUN_PORT, NULL);
    
    // Set the controlling mode
    peer->controlling = controlling;
    g_object_set(G_OBJECT(peer->agent), "controlling-mode", controlling, NULL);
    
    // Configure TURN server if credentials are provided
    if (strlen(TURN_SERVER) > 0) {
        g_object_set(G_OBJECT(peer->agent), "turn-server", TURN_SERVER, NULL);
        g_object_set(G_OBJECT(peer->agent), "turn-server-port", TURN_PORT, NULL);
        
        if (strlen(TURN_USER) > 0 && strlen(TURN_PASS) > 0) {
            g_object_set(G_OBJECT(peer->agent), "turn-username", TURN_USER, NULL);
            g_object_set(G_OBJECT(peer->agent), "turn-password", TURN_PASS, NULL);
        }
    }
    
    // Connect to the necessary signals
    g_signal_connect(G_OBJECT(peer->agent), "candidate-gathering-done", 
                    G_CALLBACK(cb_candidate_gathering_done), peer);
    g_signal_connect(G_OBJECT(peer->agent), "component-state-changed", 
                    G_CALLBACK(cb_component_state_changed), peer);
    
    // Create a new ICE stream with a single component
    peer->stream_id = nice_agent_add_stream(peer->agent, 1);
    if (peer->stream_id == 0) {
        g_warning("Failed to add ICE stream");
        return FALSE;
    }
    
    peer->component_id = 1;
    
    // Attach the receive callback
    nice_agent_attach_recv(peer->agent, peer->stream_id, peer->component_id,
                          context, cb_nice_recv, peer);
    
    // Start gathering local candidates
    if (!nice_agent_gather_candidates(peer->agent, peer->stream_id)) {
        g_warning("Failed to start candidate gathering");
        return FALSE;
    }
    
    peer->ice_state = ICE_GATHERING;
    printf("Started ICE candidate gathering\n");
    
    return TRUE;
}

// Callback when candidate gathering is done
void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer user_data) {
    peer_t *peer = (peer_t *)user_data;
    
    printf("ICE candidate gathering done\n");
    
    // Get the local SDP
    peer->local_sdp = nice_agent_generate_local_sdp(agent);
    printf("Local SDP:\n%s\n", peer->local_sdp);
    
    // In a real application, you would send this SDP to the remote peer
    // For this example, we'll require manual exchange
    printf("\n--- Copy the above SDP and send it to the remote peer ---\n");
    printf("--- Then paste the remote peer's SDP below ---\n");
    fflush(stdout);
    
    // Signal that candidate gathering is done
    g_mutex_lock(&sdp_mutex);
    got_remote_sdp = FALSE;
    g_mutex_unlock(&sdp_mutex);
}

// Callback for ICE component state changes
void cb_component_state_changed(NiceAgent *agent, guint stream_id, guint component_id, 
                              guint state, gpointer user_data) {
    peer_t *peer = (peer_t *)user_data;
    
    if (state == NICE_COMPONENT_STATE_READY) {
        printf("ICE connection established!\n");
        peer->ice_state = ICE_CONNECTED;
    } else if (state == NICE_COMPONENT_STATE_FAILED) {
        printf("ICE connection failed\n");
        peer->ice_state = ICE_FAILED;
    }
}

// Callback for receiving data
void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id, 
                guint len, gchar *buf, gpointer user_data) {
    if (len > 0) {
        // Null-terminate the message
        char *message = g_malloc0(len + 1);
        memcpy(message, buf, len);
        printf("Received: %s", message);
        g_free(message);
    }
}

// Process the remote SDP and set up the connection
gboolean process_remote_sdp(peer_t *peer, const gchar *sdp) {
    printf("Processing remote SDP...\n");
    
    if (!nice_agent_parse_remote_sdp(peer->agent, sdp)) {
        g_warning("Failed to parse remote SDP");
        return FALSE;
    }
    
    printf("Remote SDP processed, starting ICE negotiation\n");
    peer->ice_state = ICE_CONNECTING;
    
    // If we're the controlling agent, we need to start the connectivity checks
    if (peer->controlling) {
        nice_agent_gather_candidates(peer->agent, peer->stream_id);
    }
    
    return TRUE;
}

// Send a message using the ICE connection
gboolean send_ice_message(peer_t *peer, const char *message, int len) {
    if (peer->ice_state != ICE_CONNECTED) {
        printf("Cannot send message, ICE not connected\n");
        return FALSE;
    }
    
    if (nice_agent_send(peer->agent, peer->stream_id, peer->component_id, len, message) != len) {
        g_warning("Failed to send message");
        return FALSE;
    }
    
    return TRUE;
}

// Clean up ICE resources
void cleanup_ice(peer_t *peer) {
    if (peer->agent) {
        g_object_unref(peer->agent);
        peer->agent = NULL;
    }
    
    if (peer->loop) {
        g_main_loop_quit(peer->loop);
        g_thread_join(peer->thread);
        g_main_loop_unref(peer->loop);
        peer->loop = NULL;
    }
    
    g_free(peer->local_sdp);
    g_free(peer->remote_sdp);
}

// Original socket-based code (to be used as fallback)
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
            if (strlen(TURN_SERVER) == 0) {
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