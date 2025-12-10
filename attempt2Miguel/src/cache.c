#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "cache.h"

static uint64_t hash_str(const char *s) {
    uint64_t h = 146527;
    while (*s) h = (h * 33) ^ (unsigned char)*s++;
    return h;
}

static void move_to_front(cache_t *c, cache_entry_t *e) {
    if (c->head == e) return;

    // Remove from list
    if (e->prev) e->prev->next = e->next;
    if (e->next) e->next->prev = e->prev;

    if (c->tail == e){
        c->tail = e->prev;
    }

    // Insert at head
    e->prev = NULL;
    e->next = c->head;
    if (c->head){
        c->head->prev = e;
    }
    c->head = e;

    if (c->tail == NULL){
        c->tail = e;
    }
}

cache_t* cache_init(size_t max_size_mb){
    cache_t *c = calloc(1, sizeof(cache_t));
    c->max_size = max_size_mb * 1024 * 1024;
    c->table_size = 4096;
    c->table = calloc(c->table_size, sizeof(cache_entry_t*));

    if (!c->table) {
        free(c);
        return NULL;
    }

    if (pthread_rwlock_init(&c->lock, NULL) != 0) {
        free(c->table);
        free(c);
        return NULL;
    }

    return c;
}

void cache_evict_lru(cache_t *c) {
    cache_entry_t *e = c->tail;
    if (!e) return;

    // Remove from DLL
    if (e->prev){
        e->prev->next = NULL;
    }else {
        c->head = NULL;
    }

    c->tail = e->prev;

    // Remove from hash
    uint64_t h = hash_str(e->key) % c->table_size;
    cache_entry_t **p = &c->table[h];
    while (*p && *p != e){
        p = &(*p)->h_next;
    }
    if (*p){
        *p = e->h_next;
    }
    c->current_size -= e->size;

    free(e->key);
    free(e->data);
    free(e);
}

cache_entry_t* cache_get(cache_t *c, const char *key) {
    pthread_rwlock_rdlock(&c->lock);

    uint64_t h = hash_str(key) % c->table_size;
    cache_entry_t *e = c->table[h];

    while (e && strcmp(e->key, key) != 0){
        e = e->h_next;
    }
        
    if (!e) {
        pthread_rwlock_unlock(&c->lock);
        return NULL;
    }

    pthread_rwlock_unlock(&c->lock);
    pthread_rwlock_wrlock(&c->lock);

    move_to_front(c, e);

    pthread_rwlock_unlock(&c->lock);
    return e;
}

void cache_put(cache_t *c, const char *key, void *data, size_t size) {
    pthread_rwlock_wrlock(&c->lock);

    uint64_t h = hash_str(key) % c->table_size;
    cache_entry_t *e = c->table[h];
    cache_entry_t *prev = NULL;

    // If exists, replace + move to front
    while (e && strcmp(e->key, key) != 0) {
        prev = e;
        e = e->h_next;
    }

    free(prev);

    if (e) {
        // Update size
        c->current_size -= e->size;
        free(e->data);

        e->data = data;
        e->size = size;

        move_to_front(c, e);
    } else {
        // Create new entry
        e = calloc(1, sizeof(cache_entry_t));
        e->key = strdup(key);
        e->data = data;
        e->size = size;

        // Insert to hash bucket
        e->h_next = c->table[h];
        c->table[h] = e;

        // Insert to front of LRU list
        e->prev = NULL;
        e->next = c->head;
        if (c->head){
            c->head->prev = e;
        }
        c->head = e;
        if (!c->tail){
            c->tail = e;
        }
    }

    c->current_size += size;

    // Evict until size <= max
    while (c->current_size > c->max_size)
        cache_evict_lru(c);

    pthread_rwlock_unlock(&c->lock);
}

void cache_destroy(cache_t *c) {
    pthread_rwlock_wrlock(&c->lock);

    cache_entry_t *e = c->head;
    while (e) {
        cache_entry_t *next = e->next;
        free(e->key);
        free(e->data);
        free(e);
        e = next;
    }

    free(c->table);
    pthread_rwlock_unlock(&c->lock);
    pthread_rwlock_destroy(&c->lock);
    free(c);
}

