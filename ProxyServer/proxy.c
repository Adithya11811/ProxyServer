#include "client_op.h"
#include "proxy_parse.h"

int main(int argc, char *argv[]) {
  int sockfd, newsockfd;
  struct sockaddr_in serv_addr;
  struct sockaddr cli_addr;

  if (argc < 2) {
    fprintf(stderr, "Provide a port!\n");
    return 1;
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Cannot create socket");
    return 1;
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  int portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("Error on binding");
    return 1;
  }

  listen(sockfd, 100);
  socklen_t clilen = sizeof(struct sockaddr);

  printf("Proxy Server Running on port: %d\n", portno);

  while (1) {
    newsockfd = accept(sockfd, &cli_addr, &clilen);
    if (newsockfd < 0) {
      perror("Error on accepting request");
      continue;
    }

    pthread_t thread_id;
    int *client_sock = (int *)malloc(sizeof(int));
    *client_sock = newsockfd;

    if (pthread_create(&thread_id, NULL, dataFromClient, (void *)client_sock) !=
        0) {
      perror("Failed to create thread");
      close(newsockfd);
    } else {
      pthread_detach(thread_id);
    }
  }

  close(sockfd);
  return 0;
}

