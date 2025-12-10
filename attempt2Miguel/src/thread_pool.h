#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include "config.h"

typedef struct task {
    void (*function)(void*);
    void *arg;
    struct task *next;
} task_t;

typedef struct {
    pthread_t *threads;
    int num_threads;

    task_t *head;
    task_t *tail;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int shutdown;

    int task_count; 
    int max_tasks;
} thread_pool_t;

thread_pool_t* create_thread_pool(int num_threads, int queue_size);
int add_task(thread_pool_t* pool, void (*function)(void*), void* arg);
int thread_pool_dequeue(thread_pool_t* pool);
void destroy_thread_pool(thread_pool_t* pool);

#endif