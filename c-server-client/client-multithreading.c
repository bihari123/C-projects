
/*
 * filename server_ipaddress portno
 * argc[0] filename
 * argv[1] server_ipaddress
 * argv[2] portno
 */
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void error(const char *msg) {
  perror(msg);
  exit(0);
}
typedef enum { false = 0, true = 1 } terminate;
terminate should_terminate = false;

pthread_mutex_t lock;

int main(int argc, char *argv[]) {

  int sockfd, portno, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  char buffer[256];

  if (argc < 3) {
    fprintf(stderr, "usage %s hostname port\n", argv[0]);
    exit(1);
  }

  portno = atoi(argv[2]);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0)
    error("ERROR opening port");

  server = gethostbyname(argv[1]);

  if (server == NULL) {
    fprintf(stderr, "Error, no such host\n");
  }

  bzero((char *)&serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);

  serv_addr.sin_port = htons(portno);

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    error("Connection Failed");
  }

  char read_buffer[255];
  char write_buffer[255];

  while (!should_terminate) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sockfd, &write_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000; // 10 milliseconds

    int ready = select(sockfd + 1, &read_fds, &write_fds, NULL, &timeout);
    if (ready < 0) {
      error("Error in select");
    } else if (ready > 0 && FD_ISSET(sockfd, &read_fds)) {

      printf("reading the data\n");
      bzero(read_buffer, 255);
      n = read(sockfd, read_buffer, 255);

      if (n < 0)
        error("Error on reading");

      printf("SERVER: %s\n", read_buffer);

      int i = strncmp("Bye", read_buffer, 3);

      if (i == 0) {
        pthread_mutex_lock(&lock);
        should_terminate = true;
        pthread_mutex_unlock(&lock);
        break;
      }
    } else if (ready > 0 && FD_ISSET(sockfd, &write_fds)) {

      printf("writing the data\n");
      bzero(write_buffer, 255);
      fgets(write_buffer, 255, stdin);

      n = write(sockfd, write_buffer, strlen(write_buffer));

      if (n < 0)
        error("Error on writing");

      printf("You: %s\n", write_buffer);

      int i = strncmp("Bye", write_buffer, 3);

      if (i == 0) {
        pthread_mutex_lock(&lock);
        should_terminate = true;
        pthread_mutex_unlock(&lock);
        break;
      }
    }
  }
  close(sockfd);
  return 0;
}
