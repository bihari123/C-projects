#ifndef ICE_H
#define ICE_H

#include "common.h"

// ICE functions
void *glib_thread_func(void *data);
gboolean setup_ice(peer_t *peer, gboolean controlling);
gboolean process_remote_sdp(peer_t *peer, const gchar *sdp);
gboolean send_ice_message(peer_t *peer, const char *message, int len);
void cleanup_ice(peer_t *peer);

// Callback functions
void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer user_data);
void cb_component_state_changed(NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer user_data);
void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer user_data);

#endif /* ICE_H */