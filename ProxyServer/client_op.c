#include "client_op.h"

char *convert_Request_to_string(struct ParsedRequest *req) {
  printf("CONVERT TO STRING\n");
  ParsedHeader_set(req, "Host", req->host);
  ParsedHeader_set(req, "Connection", "close");

  int iHeadersLen = ParsedHeader_headersLen(req);
  char *headersBuf = (char *)malloc(iHeadersLen + 1);
  if (headersBuf == NULL) {
    fprintf(stderr, "Error in memory allocation of headersBuffer!\n");
    exit(1);
  }

  ParsedRequest_unparse_headers(req, headersBuf, iHeadersLen);
  headersBuf[iHeadersLen] = '\0';

  int request_size = strlen(req->method) + strlen(req->path) +
                     strlen(req->version) + iHeadersLen + 4;
  char *serverReq = (char *)malloc(request_size + 1);
  if (serverReq == NULL) {
    fprintf(stderr, "Error in memory allocation for server request!\n");
    exit(1);
  }

  snprintf(serverReq, request_size + 1, "%s %s %s\r\n%s", req->method,
           req->path, req->version, headersBuf);

  free(headersBuf);
  return serverReq;
}

void handleConnectRequest(int client_sock, struct ParsedRequest *req) {
  printf("Handling CONNECT request\n");
  printf("HOST: %s", req->host);
  printf("PORT: %s\n", req->port);
  // 1. Get the host and port from the req struct
  const char *host = req->host;
  const char *port =
      req->port ? req->port
                : "443"; // Default to port 443 for HTTPS if not provided

  // 2. Establish a connection to the destination server
  int server_sock = createServerSocket(host, port);
  if (server_sock < 0) {
    fprintf(stderr,
            "Failed to connect to destination server for CONNECT request\n");
    close(client_sock);
    return;
  }

  // 3. Send 200 Connection Established response to the client
  const char *connection_established =
      "HTTP/1.1 200 Connection Established\r\n\r\n";
  writeToSocket(connection_established, client_sock,
                strlen(connection_established));

  // 4. Relay data between client and server (using select for simultaneous I/O)
  fd_set fds;
  char buffer[4096];
  int max_fd = (client_sock > server_sock) ? client_sock : server_sock;

  while (1) {
    FD_ZERO(&fds);
    FD_SET(client_sock, &fds);
    FD_SET(server_sock, &fds);

    if (select(max_fd + 1, &fds, NULL, NULL, NULL) < 0) {
      perror("select error");
      break;
    }

    // Data from client to server
    if (FD_ISSET(client_sock, &fds)) {
      int bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
      if (bytes_received <= 0) {
        break; // Client closed the connection
      }
      send(server_sock, buffer, bytes_received, 0);
    }

    // Data from server to client
    if (FD_ISSET(server_sock, &fds)) {
      int bytes_received = recv(server_sock, buffer, sizeof(buffer), 0);
      if (bytes_received <= 0) {
        break; // Server closed the connection
      }
      send(client_sock, buffer, bytes_received, 0);
    }
  }

  // Close both client and server sockets
  close(server_sock);
}
void *dataFromClient(void *sockid) {
  printf("DATA FROM CLIENT\n");

  const int INITIAL_BUF_SIZE = 5000;
  int MAX_BUFFER_SIZE = INITIAL_BUF_SIZE;
  char buf[INITIAL_BUF_SIZE];
  int newsockfd = *((int *)sockid);
  free(sockid);

  char *request_message = (char *)malloc(MAX_BUFFER_SIZE);
  if (request_message == NULL) {
    fprintf(stderr, "Error in memory allocation!\n");
    exit(1);
  }

  request_message[0] = '\0'; // Initialize the buffer
  int total_received_bits = 0;

  while (strstr(request_message, "\r\n\r\n") == NULL) {
    int recvd = recv(newsockfd, buf, INITIAL_BUF_SIZE, 0);
    if (recvd < 0) {
      perror("Error while receiving");
      exit(1);
    } else if (recvd == 0) {
      break; // Client closed the connection
    } else {
      total_received_bits += recvd;
      buf[recvd] = '\0';

      if (total_received_bits > MAX_BUFFER_SIZE) {
        MAX_BUFFER_SIZE *= 2;
        request_message = (char *)realloc(request_message, MAX_BUFFER_SIZE);
        if (request_message == NULL) {
          fprintf(stderr, "Error in memory re-allocation!\n");
          exit(1);
        }
      }
    }
    strcat(request_message, buf);
  }

  if (strlen(request_message) == 0) {
    close(newsockfd);
    free(request_message);
    return 0;
  }

  struct ParsedRequest *req = ParsedRequest_create();

  if (ParsedRequest_parse(req, request_message, strlen(request_message)) < 0) {
    // printf("Request_message: %s\n", request_message);
    // printf("req->method: %s\n", req->method);
    // printf("req->host: %s\n", req->host);
    fprintf(stderr, "Error in request message. Only HTTP GET/CONNECT with "
                    "headers is allowed!\n");
  }
  if (req->port == NULL)
    req->port = strdup("80");

  if (strcmp(req->method, "GET") == 0) {
    printf("Request_message: %s\n", request_message);
    printf("req->method: %s\n", req->method);
    printf("req->host: %s\n", req->host);
    // Example DNS lookup and cache logic (replace with actual logic)
    printf("HANDLING GET REQUEST\n");
    char *url_path = extract_url_path(request_message);
    char *temp = perform_dns_lookup(url_path);

    printf("URL: %s\n", url_path);
    printf("TEMP: %s\n", temp);
    int isBlocked = 0;
    if (strcmp(url_path, temp) == 0) {
      printf("URL and TEMP are same\n");
    } else {
      isBlocked = 1;
      printf("URL and TEMP are different\n");
    }

    cache_element *cached_response = find(temp);

    if (cached_response != NULL) {
      writeToSocket(cached_response->data, newsockfd,
                    strlen(cached_response->data));
    } else {
      char *browser_req = convert_Request_to_string(req);
      int iServerfd = createServerSocket(req->host, req->port);
      if (isBlocked) {
        printf("Blocked domain: %s\n", url_path);
        writeToSocket(GOOGLE_REDIRECT, newsockfd, strlen(GOOGLE_REDIRECT));
        // writeToClient(newsockfd, iServerfd, request_message);
        printf("Redirected to %s\n", REDIRECT_IP);
      } else {
        writeToSocket(browser_req, iServerfd, strlen(browser_req));
        writeToClient(newsockfd, iServerfd, request_message);
      }

      ParsedRequest_destroy(req);
      close(iServerfd);
    }
  } else if (strcmp(req->method, "CONNECT") == 0) {
    handleConnectRequest(newsockfd, req);
    ParsedRequest_destroy(req);
  }
  close(newsockfd);
  free(request_message);

  return 0;
}