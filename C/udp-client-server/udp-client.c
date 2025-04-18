#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: %s <port> \n", argv[0]);
    exit(0);
  }

  int port = atoi(argv[1]);

  int sockfd;
  struct sockaddr_in serverAddr;
  char buffer[1024];

  socklen_t addr_size;
  sockfd = socket(PF_INET, SOCK_DGRAM, 0);

  if (sockfd < 0) {

    error("error making socket");
  }

  bzero((char *)&serverAddr, sizeof(serverAddr));

  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

  strcpy(buffer, "Hello server\n");
  sendto(sockfd, buffer, 1024, 0, (struct sockaddr *)&serverAddr,
         sizeof(serverAddr));

  // no need to bind the client to the server.
}
