#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct packt {
  char data[1024];
} Packet;

typedef struct frame {
  int frame_kind;
  int sq_no;
  int ack;
  Packet packet;
} Frame;

int main(int argc, char **argv[]) {}
