#include "../include/ice.h"

// Global variables
GMutex sdp_mutex;
GCond sdp_cond;
gboolean got_remote_sdp = FALSE;
peer_t *global_peer = NULL;

/**
 * GLib main loop thread function
 */
void *glib_thread_func(void *data) {
    GMainLoop *loop = (GMainLoop *)data;
    g_main_loop_run(loop);
    return NULL;
}

/**
 * Set up ICE connection
 */
gboolean setup_ice(peer_t *peer, gboolean controlling) {
    GMainContext *context;
    
    // Create main context and loop for the ICE agent
    context = g_main_context_new();
    peer->loop = g_main_loop_new(context, FALSE);
    
    // Start the GLib main loop in a separate thread
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

/**
 * Callback when candidate gathering is done
 */
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

/**
 * Callback for ICE component state changes
 */
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

/**
 * Callback for receiving data
 */
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

/**
 * Process the remote SDP and set up the connection
 */
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

/**
 * Send a message using the ICE connection
 */
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

/**
 * Clean up ICE resources
 */
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