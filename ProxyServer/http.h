#ifndef HTTP_H
#define HTTP_H

#include "proxy_parse.h"

typedef struct ParsedRequest ParsedRequest;
int sendErrorMessage(int socket, int status_code);
int handle_request(int clientSocket, ParsedRequest *request, char *tempReq);
int checkHTTPversion(char *msg);

#endif
