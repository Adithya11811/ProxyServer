#include "hashCache.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include "uthash.h"

cache_element *cache = NULL;
int cache_size = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // Initialize mutex

char *extract_url_path(char *url)
{
    char *start = strstr(url, "GET ");
    char *end = strstr(url, " HTTP/");
    if (start && end && end > start)
    {
        size_t length = end - start - 4;
        char *path = (char *)malloc(length + 1);
        if (path == NULL)
        {
            perror("Memory allocation for URL path failed");
            return NULL;
        }
        strncpy(path, start + 4, length);
        path[length] = '\0';
        return path;
    }
    return NULL;
}

void log_to_flask_server(const char *log)
{
    int sock;
    struct sockaddr_in server_addr;
    char server_ip[] = "127.0.0.1"; // Flask server IP
    int server_port = 5000;         // Flask server port

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }

    // Connect to Flask server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Prepare the HTTP POST request
    char request[2048];
    snprintf(request, sizeof(request),
             "POST /log HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Content-Length: %ld\r\n"
             "\r\n"
             "log=%s",
             server_ip, server_port, strlen(log) + 4, log);

    // Send the request
    if (send(sock, request, strlen(request), 0) < 0)
    {
        perror("Send failed");
    }

    // Close the socket
    close(sock);
}

void log_cache_element(const char *header, cache_element *element)
{
    if (element == NULL)
        return;

    char log[2048];
    snprintf(log, sizeof(log),
             "%s\n"
             "URL: %s\n"
             "Data: %s\n"
             "Length: %d\n"
             "LRU Time Track: %ld\n",
             header, element->url, element->data, element->len, element->lru_time_track);
    printf("\n --- --- --- \n");
    printf("%s\n", log);
    printf("\n --- --- --- \n");
    log_to_flask_server(log);
}

cache_element *find(char *url_path)
{
    cache_element *site = NULL;

    if (!url_path)
    {
        printf("Failed to extract URL path\n");
        return NULL;
    }

    int temp_lock_val = pthread_mutex_lock(&lock);
    if (temp_lock_val != 0)
    {
        fprintf(stderr, "Error acquiring cache lock: %d\n", temp_lock_val);
    }

    HASH_FIND_STR(cache, url_path, site);
    if (site != NULL)
    {
        printf("LRU Time Track Before: %ld\n", site->lru_time_track);
        printf("\nURL found\n");
        site->lru_time_track = time(NULL);
        printf("LRU Time Track After: %ld\n", site->lru_time_track);
    }
    else
    {
        printf("\nURL not found\n");
    }

    free(url_path);

    temp_lock_val = pthread_mutex_unlock(&lock);
    if (temp_lock_val != 0)
    {
        fprintf(stderr, "Error releasing cache lock: %d\n", temp_lock_val);
    }
    return site;
}

void remove_cache_element()
{
    printf("REMOVE CACHE ELEMENT\n");

    cache_element *site = NULL;
    cache_element *oldest = NULL;

    int temp_lock_val = pthread_mutex_lock(&lock);
    if (temp_lock_val != 0)
    {
        fprintf(stderr, "Error acquiring cache lock: %d\n", temp_lock_val);
    }

    if (cache != NULL)
    {
        for (site = cache; site != NULL; site = (cache_element *)site->hh.next)
        {
            if (oldest == NULL || site->lru_time_track < oldest->lru_time_track)
            {
                oldest = site;
            }
        }
        if (oldest != NULL)
        {
            HASH_DEL(cache, oldest);
            log_cache_element("Removed", oldest);                                          // Log the removed cache element
            cache_size -= (oldest->len) + sizeof(cache_element) + strlen(oldest->url) + 1; // Update cache size
            free(oldest->data);
            free(oldest->url); // Free the removed element
            free(oldest);
        }
    }

    temp_lock_val = pthread_mutex_unlock(&lock);
    if (temp_lock_val != 0)
    {
        fprintf(stderr, "Error releasing cache lock: %d\n", temp_lock_val);
    }
}

int add_cache_element(char *data, int size, char *url)
{
    // printf("\n --- --- --- \n");
    // printf("URL PATH: %s\n", url);
    // printf("\n --- --- --- \n");
    printf("ADD CACHE ELEMENT\n");

    if (pthread_mutex_lock(&lock) != 0)
    {
        fprintf(stderr, "Error acquiring cache lock\n");
        return 0;
    }

    int element_size = size + 1 + strlen(url) + sizeof(cache_element);
    printf("Element size: %d, MAX_ELEMENT_SIZE: %d\n", element_size, MAX_ELEMENT_SIZE);

    if (element_size > MAX_ELEMENT_SIZE)
    {
        printf("Element size is greater than MAX_ELEMENT_SIZE. Not caching.\n");
        pthread_mutex_unlock(&lock);
        return 0;
    }

    while (cache_size + element_size > MAX_SIZE)
    {
        remove_cache_element();
    }

    cache_element *element = (cache_element *)malloc(sizeof(cache_element));
    if (element == NULL)
    {
        perror("Memory allocation for cache element failed");
        pthread_mutex_unlock(&lock);
        return 0;
    }

    element->data = (char *)malloc(size + 1);
    if (element->data == NULL)
    {
        perror("Memory allocation for cache data failed");
        free(element);
        pthread_mutex_unlock(&lock);
        return 0;
    }

    memcpy(element->data, data, size);
    element->data[size] = '\0';
    char *url_path = extract_url_path(url);
    element->url = url_path;
    if (element->url == NULL)
    {
        perror("Memory allocation for cache URL failed");
        free(element->data);
        free(element);
        pthread_mutex_unlock(&lock);
        return 0;
    }

    element->lru_time_track = time(NULL);
    HASH_ADD_STR(cache, url, element);
    element->len = size;
    cache_size += element_size;
    log_cache_element("Added", element); // Log the added cache element

    if (pthread_mutex_unlock(&lock) != 0)
    {
        fprintf(stderr, "Error releasing cache lock\n");
    }

    printf("END OF ADD CACHE\n");
    return 1;
}