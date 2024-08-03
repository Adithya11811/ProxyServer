#ifndef HASHCACHE_H
#define HASHCACHE_H

#include <stdlib.h>
#include <time.h>
#include "uthash.h"

typedef struct cache_element
{
    char *url;                  // Key
    char *data;                 // Data
    int len;                    // Data length
    time_t lru_time_track;      // LRU time tracker
    UT_hash_handle hh;          // Makes this structure hashable
} cache_element;

#define MAX_SIZE 1048576        // 1MB max size
#define MAX_ELEMENT_SIZE 102400 // 100KB max element size

extern pthread_mutex_t lock;
void log_cache_element(const char *header, cache_element *element);
char *extract_url_path(char *url);
void log_to_flask_server(const char* log);
cache_element *find(char *url);
void remove_cache_element();
int add_cache_element(char *data, int size, char *url);

#endif
