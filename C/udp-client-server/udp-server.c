#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

void error(const char *msg) {
  perror(msg);
  exit(1);
}
int main(int argc, char **argv) {

  if (argc != 2) {
    printf("USAGE: %s <port>\n", argv[0]);
    exit(0);
  }

  int port = atoi(argv[1]);
  int sockfd;
  struct sockaddr_in si_me, si_other;
  char buffer[1024];
  socklen_t addr_size;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  if (socket < 0) {
    error("error opening file");
  }

  bzero((char *)&si_me, sizeof(si_me));

  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(port);
  si_me.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (bind(sockfd, (struct sockaddr *)&si_me, sizeof(si_me)) < 0) {
    error("binding failed");
  }

  addr_size = sizeof(si_other);

  if (recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr *)&si_other,
               &addr_size) < 0) {
    error("error reading the data");
  }

  printf("[+] Data recieved: %s", buffer);

  return 0;
}
