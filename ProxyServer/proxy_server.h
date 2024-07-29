#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include "proxy_parse.h"
#include "cache.h"
#include "http.h"
#include "utils.h"

#define MAX_BYTES 4096
#define MAX_CLIENTS 400

typedef struct ParsedRequest ParsedRequest;
extern int port_number;
extern int proxy_socketId;
extern pthread_t tid[MAX_CLIENTS];
extern sem_t semaphore;

void *thread_fn(void *socketNew);

#endif
