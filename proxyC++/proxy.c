#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/wait.h>
#include <netdb.h>

#include "hashCache.h"
#include "proxy_parse.h"

char *convert_Request_to_string(struct ParsedRequest *req)
{
    printf("CONVERT TO STRING\n");
    ParsedHeader_set(req, "Host", req->host);
    ParsedHeader_set(req, "Connection", "close");

    int iHeadersLen = ParsedHeader_headersLen(req);
    char *headersBuf = (char *)malloc(iHeadersLen + 1);
    if (headersBuf == NULL)
    {
        fprintf(stderr, "Error in memory allocation of headersBuffer!\n");
        exit(1);
    }

    ParsedRequest_unparse_headers(req, headersBuf, iHeadersLen);
    headersBuf[iHeadersLen] = '\0';

    int request_size = strlen(req->method) + strlen(req->path) + strlen(req->version) + iHeadersLen + 4;
    char *serverReq = (char *)malloc(request_size + 1);
    if (serverReq == NULL)
    {
        fprintf(stderr, "Error in memory allocation for server request!\n");
        exit(1);
    }

    snprintf(serverReq, request_size + 1, "%s %s %s\r\n%s", req->method, req->path, req->version, headersBuf);

    free(headersBuf);
    return serverReq;
}

int createServerSocket(const char *pcAddress, const char *pcPort)
{
    printf("CREATE SERVER SOCKET\n");
    struct addrinfo ahints, *paRes;
    int iSockfd;

    memset(&ahints, 0, sizeof(ahints));
    ahints.ai_family = AF_UNSPEC;
    ahints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(pcAddress, pcPort, &ahints, &paRes) != 0)
    {
        fprintf(stderr, "Error in server address format!\n");
        exit(1);
    }

    if ((iSockfd = socket(paRes->ai_family, paRes->ai_socktype, paRes->ai_protocol)) < 0)
    {
        fprintf(stderr, "Error in creating socket to server!\n");
        exit(1);
    }
    if (connect(iSockfd, paRes->ai_addr, paRes->ai_addrlen) < 0)
    {
        fprintf(stderr, "Error in connecting to server!\n");
        exit(1);
    }

    freeaddrinfo(paRes);
    return iSockfd;
}

void writeToSocket(const char *buffer, int sockfd, int buffer_length)
{
    printf("WRITE TO SOCKET\n");

    int totalsent = 0, senteach;

    while (totalsent < buffer_length)
    {
        senteach = send(sockfd, buffer + totalsent, buffer_length - totalsent, 0);
        if (senteach < 0)
        {
            perror("Error in sending");
            exit(1);
        }
        totalsent += senteach;
    }
}

void writeToClient(int Clientfd, int Serverfd, char *url)
{
    printf("WRITE TO CLIENT\n");

    const int INITIAL_BUF_SIZE = 5000;
    int iRecv, current_buf_size = INITIAL_BUF_SIZE;
    char buf[INITIAL_BUF_SIZE];
    char *server_response_data = (char *)malloc(current_buf_size);

    if (server_response_data == NULL)
    {
        fprintf(stderr, "Error in memory allocation for server response!\n");
        exit(1);
    }

    int total_response_size = 0;

    while ((iRecv = recv(Serverfd, buf, INITIAL_BUF_SIZE, 0)) > 0)
    {
        writeToSocket(buf, Clientfd, iRecv);

        if (total_response_size + iRecv > current_buf_size)
        {
            current_buf_size *= 2;
            server_response_data = (char *)realloc(server_response_data, current_buf_size);
            if (server_response_data == NULL)
            {
                fprintf(stderr, "Error in memory re-allocation for server response!\n");
                exit(1);
            }
        }

        memcpy(server_response_data + total_response_size, buf, iRecv);
        total_response_size += iRecv;
    }

    if (iRecv < 0)
    {
        fprintf(stderr, "Error while receiving from server!\n");
        exit(1);
    }

    add_cache_element(server_response_data, total_response_size, url);

    free(server_response_data);
}

void *dataFromClient(void *sockid)
{
    printf("DATA FROM CLIENT\n");

    const int INITIAL_BUF_SIZE = 5000;
    int MAX_BUFFER_SIZE = INITIAL_BUF_SIZE;
    char buf[INITIAL_BUF_SIZE];
    int newsockfd = *((int *)sockid);
    free(sockid);

    char *request_message = (char *)malloc(MAX_BUFFER_SIZE);

    if (request_message == NULL)
    {
        fprintf(stderr, "Error in memory allocation!\n");
        exit(1);
    }

    request_message[0] = '\0';
    int total_received_bits = 0;

    while (strstr(request_message, "\r\n\r\n") == NULL)
    {
        int recvd = recv(newsockfd, buf, INITIAL_BUF_SIZE, 0);
        if (recvd < 0)
        {
            perror("Error while receiving");
            exit(1);
        }
        else if (recvd == 0)
        {
            break;
        }
        else
        {
            total_received_bits += recvd;
            buf[recvd] = '\0';

            if (total_received_bits > MAX_BUFFER_SIZE)
            {
                MAX_BUFFER_SIZE *= 2;
                request_message = (char *)realloc(request_message, MAX_BUFFER_SIZE);
                if (request_message == NULL)
                {
                    fprintf(stderr, "Error in memory re-allocation!\n");
                    exit(1);
                }
            }
        }
        strcat(request_message, buf);
    }

    if(strlen(request_message) == 0)
    {
        close(newsockfd);
        free(request_message);
        return NULL;
    }

    struct ParsedRequest *req = ParsedRequest_create();

    if (ParsedRequest_parse(req, request_message, strlen(request_message)) < 0)
    {
        fprintf(stderr, "Error in request message. Only HTTP GET with headers is allowed!\n");
        exit(0);
    }

    if (req->port == NULL)
        req->port = strdup("80");

    cache_element *cached_response = find(request_message);

    if (cached_response != NULL)
    {
        writeToSocket(cached_response->data, newsockfd, strlen(cached_response->data));
    }
    else
    {
        char *browser_req = convert_Request_to_string(req);
        int iServerfd = createServerSocket(req->host, req->port);

        writeToSocket(browser_req, iServerfd, strlen(browser_req));
        writeToClient(newsockfd, iServerfd, request_message);

        ParsedRequest_destroy(req);
        close(iServerfd);
    }

    close(newsockfd);
    free(request_message);

    int *ret_val = (int *)malloc(sizeof(int));
    *ret_val = 0;
    return ret_val;
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr;
    struct sockaddr cli_addr;

    if (argc < 2)
    {
        fprintf(stderr, "Provide a port!\n");
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Cannot create socket");
        return 1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    int portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Error on binding");
        return 1;
    }

    listen(sockfd, 100);
    socklen_t clilen = sizeof(struct sockaddr);

    printf("Proxy Server Running on port: %d\n", portno);

    while (1)
    {
        newsockfd = accept(sockfd, &cli_addr, &clilen);
        if (newsockfd < 0)
        {
            perror("Error on accepting request");
            continue;
        }

        pthread_t thread_id;
        int *client_sock = (int *)malloc(sizeof(int));
        *client_sock = newsockfd;

        if (pthread_create(&thread_id, NULL, dataFromClient, (void *)client_sock) != 0)
        {
            perror("Failed to create thread");
            close(newsockfd);
        }
        else
        {
            pthread_detach(thread_id);
        }
    }

    close(sockfd);
    return 0;
}
