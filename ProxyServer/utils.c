#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

int connectRemoteServer(char *host_addr, int port_num)
{
    // Creating Socket for remote server ---------------------------

    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (remoteSocket < 0)
    {
        printf("Error in Creating Socket.\n");
        return -1;
    }

    // Get host by the name or ip address provided

    struct hostent *host = gethostbyname(host_addr);
    if (host == NULL)
    {
        fprintf(stderr, "No such host exists.\n");
        return -1;
    }

    // inserts ip address and port number of host in struct `server_addr`
    struct sockaddr_in server_addr;

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);

    bcopy((char *)host->h_addr_list[0], (char *)&server_addr.sin_addr.s_addr, host->h_length);

    // Connect to Remote server ----------------------------------------------------

    if (connect(remoteSocket, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "Error in connecting !\n");
        return -1;
    }
    // free(host_addr);
    return remoteSocket;
}
