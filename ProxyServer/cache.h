#ifndef CACHE_H
#define CACHE_H

#include <time.h>
#include <pthread.h>

#define MAX_SIZE 200 * (1 << 20)
#define MAX_ELEMENT_SIZE 65536

typedef struct cache_element cache_element;

struct cache_element
{
    char *data;
    int len;
    char *url;
    time_t lru_time_track;
    cache_element *next;
};

extern cache_element *head;
extern int cache_size;
extern pthread_mutex_t lock;

cache_element *find(char *url);
int add_cache_element(char *data, int size, char *url);
void remove_cache_element();

#endif
