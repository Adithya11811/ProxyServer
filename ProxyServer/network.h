#ifndef NETWORK_H
#define NETWORK_H

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int createServerSocket(const char *pcAddress, const char *pcPort);
void writeToSocket(const char *buffer, int sockfd, int buffer_length);
char *perform_dns_lookup(const char *url);
void writeToClient(int Clientfd, int Serverfd, char *url);
void extract_hostname(const char *url, char *hostname);
int file_exists(const char *filename);

#endif // NETWORK_H
