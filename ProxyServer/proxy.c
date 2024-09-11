#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/wait.h>
#include <netdb.h>
#include <sys/stat.h>
#include "hashCache.h"
#include "proxy_parse.h"

#define REDIRECT_IP "/https://guthib.com/"
#define GOOGLE_REDIRECT "HTTP/1.1 302 Found\r\nLocation: https://guthib.com/\r\n\r\n"
#define BLOCKED_IP_FILE "blocked_ips.txt"

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

int file_exists(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return 1;  // File exists
    }
    return 0;  // File doesn't exist
}

// Function to add GitHub's IPs to the file if not present
void add_github_ips_to_file(FILE *file) {
    const char *github_ipv4 = "20.207.73.82";
    const char *github_ipv6 = "64:ff9b::14cf:4952";
    fprintf(file, "%s\n", github_ipv4);
    fprintf(file, "%s\n", github_ipv6);
    fflush(file);
    printf("Added GitHub's IPv4 and IPv6 addresses to the file.\n");
}

// Function to read blocked IPs from file
void read_blocked_ips(char blocked_ips[][50], int *ip_count) {
    FILE *file = fopen(BLOCKED_IP_FILE, "r");
    if (!file) {
        perror("Error opening blocked IPs file");
        return;
    }

    char line[50];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';  // Remove newline character
        strcpy(blocked_ips[*ip_count], line);
        (*ip_count)++;
    }

    fclose(file);
}

int is_ip_blocked(const char *ip) {
    char blocked_ips[100][50];  // Store up to 100 IPs
    int ip_count = 0;

    // Check if blocked IP file exists, otherwise create it
    if (!file_exists(BLOCKED_IP_FILE)) {
        printf("Blocked IP file not found. Creating %s and adding GitHub's IPs...\n", BLOCKED_IP_FILE);
        FILE *file = fopen(BLOCKED_IP_FILE, "w");
        if (!file) {
            perror("Error creating blocked IP file");
            return EXIT_FAILURE;
        }
        add_github_ips_to_file(file);
        fclose(file);
    }

    // Now, read the blocked IPs into the array
    read_blocked_ips(blocked_ips, &ip_count);

    for (int i = 0; i < ip_count; i++) {
        if (strcmp(blocked_ips[i], ip) == 0) {
            return 1; // IP is blocked
        }
    }
    return 0; // IP is not blocked
}

void extract_hostname(const char *url, char *hostname) {
    const char *start = strstr(url, "://");
    if (start) {
        start += 3; // Skip "://"
    } else {
        start = url; // No "://", start from the beginning
    }

    const char *end = strchr(start, '/'); // Find the first slash after the protocol
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

char* perform_dns_lookup(const char *url) {
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
            result_ip = strdup(REDIRECT_IP);  // Allocate memory and copy the redirect IP
            break;
        } else {
            printf("Accepted IP: %s\n", ipstr);
            result_ip = strdup(url);
            return  result_ip; // Allocate memory and copy the accepted IP
            break;
        }
    }

    freeaddrinfo(res); // Free the linked list
    return result_ip; // Caller must free the returned string
}


void *dataFromClient(void *sockid)
{
    printf("DATA FROM CLIENT\n");

    const int INITIAL_BUF_SIZE = 5000;
    int MAX_BUFFER_SIZE = INITIAL_BUF_SIZE;
    char buf[INITIAL_BUF_SIZE];
    int newsockfd = *((int *)sockid);
    free(sockid);

    // extra add madidhu
    // struct sockaddr_in addr;
    // socklen_t addr_len = sizeof(addr);
    // char client_ip[INET_ADDRSTRLEN];

    // if (getpeername(newsockfd, (struct sockaddr *)&addr, &addr_len) == -1) {
    //     perror("getpeername failed");
    //     close(newsockfd);
    //     pthread_exit(NULL);
    // }

    // inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));

    // if(is_ip_blocked(client_ip)) {
    //     printf("Blocked IP: %s\n", client_ip);
    // }else{
    //     printf("Accepted IP: %s\n", client_ip);
    // }

    //till here extra

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
        return 0;
    }

    struct ParsedRequest *req = ParsedRequest_create();

    if (ParsedRequest_parse(req, request_message, strlen(request_message)) < 0)
    {
        printf("Request_message: %s\n", request_message);
        printf("req->method: %s\n", req->method);
        printf("req->host: %s\n", req->host);
        printf("req->buf: %s\n", req->buf);
        fprintf(stderr, "Error in request message. Only HTTP GET with headers is allowed!\n");
        exit(0);
    }

    if (req->port == NULL)
        req->port = strdup("80");


    char *url_path = extract_url_path(request_message);
    char *temp = perform_dns_lookup(url_path);

    printf("URL: %s\n", url_path);
    printf("TEMP: %s\n", temp);
    int isBlocked = 0;
    if(strcmp(url_path, temp) == 0){
        printf("URL and TEMP are same\n");
    }else{
        isBlocked = 1;
        printf("URL and TEMP are different\n");
    }

    //till here extra

    cache_element *cached_response = find(temp);

    if (cached_response != NULL)
    {
        writeToSocket(cached_response->data, newsockfd, strlen(cached_response->data));
    }
    else
    {
        char *browser_req = convert_Request_to_string(req);
        int iServerfd = createServerSocket(req->host, req->port);
        if(isBlocked){
            printf("Blocked domain: %s\n", url_path);
            writeToSocket(GOOGLE_REDIRECT, newsockfd, strlen(GOOGLE_REDIRECT));
            // writeToClient(newsockfd, iServerfd, request_message);
            printf("Redirected to %s\n", REDIRECT_IP);
        }else{
            writeToSocket(browser_req, iServerfd, strlen(browser_req));
            writeToClient(newsockfd, iServerfd, request_message);
        }



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
