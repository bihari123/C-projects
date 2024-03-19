
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
void error(const char *msg) {
  perror(msg);
  exit(1);
}

typedef enum { false = 0, true = 1 } terminate;
terminate should_terminate = false;

pthread_mutex_t lock;

int main(int argc, char *argv[]) {

  if (argc < 2) {
    fprintf(stderr, "Port No not provided");
    exit(1);
  }

  int sockfd, newsockfd, portno, n;

  struct sockaddr_in serv_addr, cli_addr;
  socklen_t clilen;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    error("Error opening socket");
  }

  bzero((char *)&serv_addr, sizeof(serv_addr));

  portno = atoi(argv[1]);

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    error("binding failed");
  }

  listen(sockfd, 5);
  clilen = sizeof(cli_addr);

  newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

  if (newsockfd < 0)
    error("Error on accept");

  char read_buffer[255];
  char write_buffer[255];

  while (!should_terminate) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(newsockfd, &read_fds);

    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(newsockfd, &write_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000; // 10 milliseconds

    int ready = select(newsockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready < 0) {
      error("Error in select");
    } else if (ready > 0 && FD_ISSET(newsockfd, &read_fds)) {

      printf("reading the data\n");
      bzero(read_buffer, 255);
      n = read(newsockfd, read_buffer, 255);

      if (n < 0)
        error("Error on reading");

      printf("CLIENT: %s\n", read_buffer);

      int i = strncmp("Bye", read_buffer, 3);

      if (i == 0) {
        pthread_mutex_lock(&lock);
        should_terminate = true;
        pthread_mutex_unlock(&lock);
        break;
      }
    } else if (ready > 0 && FD_ISSET(newsockfd, &write_fds)) {

      printf("writing the data\n");
      bzero(write_buffer, 255);
      fgets(write_buffer, 255, stdin);

      n = write(newsockfd, write_buffer, strlen(write_buffer));

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

  return 0;
}
