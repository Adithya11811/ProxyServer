#include "network.h"
#include "constants.h"
#include "hashCache.h"
#include "ip_block.h"
#include "proxy_parse.h"
#include <netdb.h>

int createServerSocket(const char *pcAddress, const char *pcPort) {
  printf("CREATE SERVER SOCKET\n");
  struct addrinfo ahints, *paRes;
  int iSockfd;

  memset(&ahints, 0, sizeof(ahints));
  ahints.ai_family = AF_UNSPEC;
  ahints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(pcAddress, pcPort, &ahints, &paRes) != 0) {
    fprintf(stderr, "Error in server address format!\n");
    exit(1);
  }

  if ((iSockfd = socket(paRes->ai_family, paRes->ai_socktype,
                        paRes->ai_protocol)) < 0) {
    fprintf(stderr, "Error in creating socket to server!\n");
    exit(1);
  }
  if (connect(iSockfd, paRes->ai_addr, paRes->ai_addrlen) < 0) {
    fprintf(stderr, "Error in connecting to server!\n");
    exit(1);
  }

  freeaddrinfo(paRes);
  return iSockfd;
}

void writeToSocket(const char *buffer, int sockfd, int buffer_length) {
  printf("WRITE TO SOCKET\n");

  int totalsent = 0, senteach;

  while (totalsent < buffer_length) {
    senteach = send(sockfd, buffer + totalsent, buffer_length - totalsent, 0);
    if (senteach < 0) {
      perror("Error in sending");
      exit(1);
    }
    totalsent += senteach;
  }
}
void writeToClient(int Clientfd, int Serverfd, char *url) {
  printf("WRITE TO CLIENT\n");

  const int INITIAL_BUF_SIZE = 5000;
  int iRecv, current_buf_size = INITIAL_BUF_SIZE;
  char buf[INITIAL_BUF_SIZE];
  char *server_response_data = (char *)malloc(current_buf_size);

  if (server_response_data == NULL) {
    fprintf(stderr, "Error in memory allocation for server response!\n");
    exit(1);
  }

  int total_response_size = 0;

  while ((iRecv = recv(Serverfd, buf, INITIAL_BUF_SIZE, 0)) > 0) {
    writeToSocket(buf, Clientfd, iRecv);

    if (total_response_size + iRecv > current_buf_size) {
      current_buf_size *= 2;
      server_response_data =
          (char *)realloc(server_response_data, current_buf_size);
      if (server_response_data == NULL) {
        fprintf(stderr, "Error in memory re-allocation for server response!\n");
        exit(1);
      }
    }

    memcpy(server_response_data + total_response_size, buf, iRecv);
    total_response_size += iRecv;
  }

  if (iRecv < 0) {
    fprintf(stderr, "Error while receiving from server!\n");
    exit(1);
  }

  add_cache_element(server_response_data, total_response_size, url);

  free(server_response_data);
}

void extract_hostname(const char *url, char *hostname) {
  const char *start = strstr(url, "://");
  if (start) {
    start += 3; // Skip "://"
  } else {
    start = url; // No "://", start from the beginning
  }

  const char *end =
      strchr(start, '/'); // Find the first slash after the protocol
  if (end) {
    strncpy(hostname, start, end - start);
    hostname[end - start] = '\0';
  } else {
    strcpy(hostname, start); // No path, the whole string is the hostname
  }

  // Remove any trailing port number
  char *colon = strchr(hostname, ':');
  if (colon) {
    *colon = '\0';
  }
}

char *perform_dns_lookup(const char *url) {
  struct addrinfo hints, *res, *p;
  int status;
  char ipstr[INET6_ADDRSTRLEN];
  const char *ipver;
  char hostname[256];
  char *result_ip = NULL;

  // Extract hostname from the URL
  extract_hostname(url, hostname);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    return NULL;
  }

  printf("IP addresses for %s:\n", hostname);

  for (p = res; p != NULL; p = p->ai_next) {
    void *addr;

    // Get the pointer to the address itself
    if (p->ai_family == AF_INET) { // IPv4
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
      addr = &(ipv4->sin_addr);
      ipver = "IPv4";
    } else { // IPv6
      struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
      addr = &(ipv6->sin6_addr);
      ipver = "IPv6";
    }

    // Convert the IP to a string
    inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
    printf("  %s: %s\n", ipver, ipstr);

    if (is_ip_blocked(ipstr)) {
      printf("Blocked IP: %s, Redirecting to %s\n", ipstr, REDIRECT_IP);
      result_ip =
          strdup(REDIRECT_IP); // Allocate memory and copy the redirect IP
      break;
    } else {
      printf("Accepted IP: %s\n", ipstr);
      result_ip = strdup(url);
      return result_ip; // Allocate memory and copy the accepted IP
      break;
    }
  }

  freeaddrinfo(res); // Free the linked list
  return result_ip;  // Caller must free the returned string
}
