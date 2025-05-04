#ifndef SOCKET_H
#define SOCKET_H

#include "common.h"

// Socket communication functions
int create_server(int port);
peer_t connect_to_peer(const char *ip, int port);
void *receive_messages(void *arg);

#endif /* SOCKET_H */