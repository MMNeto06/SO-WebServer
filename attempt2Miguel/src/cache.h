#ifndef CACHE_H
#define CACHE_H 

#include <stdint.h>
#include <pthread.h>

typedef struct cache_entry {
    char *key;
    void *data;
    size_t size;

    struct cache_entry *prev;
    struct cache_entry *next;

    struct cache_entry *h_next; // hash chain
} cache_entry_t;

typedef struct cache {
    size_t max_size;
    size_t current_size;

    cache_entry_t **table;
    size_t table_size;

    cache_entry_t *head;  // MRU
    cache_entry_t *tail;  // LRU

    pthread_rwlock_t lock;
} cache_t;

cache_t* cache_init(size_t max_size_mb); 
cache_entry_t* cache_get(cache_t *cache, const char *key); 
void cache_put(cache_t *cache, const char *key, void *data, size_t size); 
void cache_evict_lru(cache_t *cache); 
void cache_destroy(cache_t *cache);

#endif

