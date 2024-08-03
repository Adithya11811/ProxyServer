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
pthread_mutex_t lock;

// Function to extract the essential part of the URL for comparison
char *extract_url_path(char *url)
{
    char *start = strstr(url, "GET ");
    char *end = strstr(url, " HTTP/");
    if (start && end && end > start)
    {
        size_t length = end - start - 4;
        char *path = (char *)malloc(length + 1);
        strncpy(path, start + 4, length);
        path[length] = '\0';
        return path;
    }
    return NULL;
}

void log_to_flask_server(const char* log) {
    int sock;
    struct sockaddr_in server_addr;
    char server_ip[] = "127.0.0.1"; // Flask server IP
    int server_port = 5000;         // Flask server port

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }

    // Connect to Flask server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
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
    send(sock, request, strlen(request), 0);

    // Close the socket
    close(sock);
}

void log_cache_element(const char *header, cache_element *element)
{
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

cache_element *find(char *url)
{
    // Checks for URL in the cache; if found, returns a pointer to the respective cache element, else returns NULL
    cache_element *site = NULL;

    char *url_path = extract_url_path(url);
    if (!url_path)
    {
        printf("Failed to extract URL path\n");
        return NULL;
    }

    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Find Cache Lock Acquired %d\n", temp_lock_val);
    HASH_FIND_STR(cache, url_path, site);
    if (site != NULL)
    {
        printf("LRU Time Track Before: %ld\n", site->lru_time_track);
        printf("\nURL found\n");
        // Updating the time_track
        site->lru_time_track = time(NULL);
        printf("LRU Time Track After: %ld\n", site->lru_time_track);
    }
    else
    {
        printf("\nURL not found\n");
    }
    free(url_path);

    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Find Cache Lock Unlocked %d\n", temp_lock_val);
    return site;
}

void remove_cache_element()
{
    // If cache is not empty, searches for the node with the least lru_time_track and deletes it
    cache_element *site = NULL;
    cache_element *oldest = NULL;

    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Remove Cache Lock Acquired %d\n", temp_lock_val);
    if (cache != NULL)
    {
        for (site = cache; site != NULL; site = site->hh.next)
        {
            if (oldest == NULL || site->lru_time_track < oldest->lru_time_track)
            {
                oldest = site;
            }
        }
        if (oldest != NULL)
        {
            HASH_DEL(cache, oldest);
            log_cache_element("Removed", oldest); // Log the removed cache element
            cache_size = cache_size - (oldest->len) - sizeof(cache_element) - strlen(oldest->url) - 1; // updating the cache size
            free(oldest->data);
            free(oldest->url); // Free the removed element
            free(oldest);
        }
    }
    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Remove Cache Lock Unlocked %d\n", temp_lock_val);
}

int add_cache_element(char *data, int size, char *url)
{
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Add Cache Lock Acquired %d\n", temp_lock_val);
    int element_size = size + 1 + strlen(url) + sizeof(cache_element); // Size of the new element which will be added to the cache
    printf("Element size: %d, MAX_ELEMENT_SIZE: %d\n", element_size, MAX_ELEMENT_SIZE);
    if (element_size > MAX_ELEMENT_SIZE)
    {
        printf("Element size is greater than MAX_ELEMENT_SIZE. Not caching.\n");
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add Cache Lock Unlocked %d\n", temp_lock_val);
        return 0;
    }
    else
    {
        while (cache_size + element_size > MAX_SIZE)
        {
            remove_cache_element();
        }
        cache_element *element = (cache_element *)malloc(sizeof(cache_element)); // Allocating memory for the new cache element
        element->data = (char *)malloc(size + 1);                                // Allocating memory for the response to be stored in the cache element
        strcpy(element->data, data);
        element->url = strdup(url);                                              // Allocating memory for the request to be stored in the cache element (as a key)
        element->lru_time_track = time(NULL);                                    // Updating the time_track
        HASH_ADD_STR(cache, url, element);
        element->len = size;
        cache_size += element_size;
        log_cache_element("Added", element); // Log the added cache element
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add Cache Lock Unlocked %d\n", temp_lock_val);
        return 1;
    }
    return 0;
}
