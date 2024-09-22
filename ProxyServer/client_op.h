#ifndef CLIENT_OP_H
#define CLIENT_OP_H

#include "constants.h"
#include "hashCache.h"
#include "network.h"
#include "proxy_parse.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void handleConnectRequest(int client_sock, struct ParsedRequest *req);
char *convert_Request_to_string(struct ParsedRequest *req);
void *dataFromClient(void *sockid);
#endif // PROXY_H
