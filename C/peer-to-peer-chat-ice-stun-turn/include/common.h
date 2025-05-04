#ifndef COMMON_H
#define COMMON_H

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

// Peer structure
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
extern GMutex sdp_mutex;
extern GCond sdp_cond;
extern gboolean got_remote_sdp;
extern peer_t *global_peer;

#endif /* COMMON_H */