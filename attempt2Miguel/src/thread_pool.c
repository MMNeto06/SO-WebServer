#include "thread_pool.h"
#include "worker.h"
#include <stdlib.h>

thread_pool_t* create_thread_pool(int num_threads, int queue_size) {
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    pool->num_threads = num_threads;

    pool->head = pool->tail = NULL;
    pool->shutdown = 0;

    pool->task_count = 0; 
    pool->max_tasks = queue_size;

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, pool);
    }

    return pool;
}

int add_task(thread_pool_t* pool, void (*function)(void*), void* arg) {
    task_t* task = malloc(sizeof(task_t));
    task->function = function;
    task->arg = arg;
    task->next = NULL;

    pthread_mutex_lock(&pool->mutex);

    if (pool->task_count >= pool->max_tasks){
        pthread_mutex_unlock(&pool->mutex);
        return 1;
    }

    if (pool->tail == NULL) {
        pool->head = pool->tail = task;
    } else {
        pool->tail->next = task;
        pool->tail = task;
    }

    pool->task_count++;

    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

int thread_pool_dequeue(thread_pool_t* pool){
    pthread_mutex_lock(&pool->mutex);

    while (!pool->shutdown && pool->head == NULL) {
        pthread_cond_wait(&pool->cond, &pool->mutex);
    }

    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->mutex);
        return 0;
    }
    
    task_t* task = pool->head;
    pool->head = task->next;
    if (pool->head == NULL) pool->tail = NULL;
    pool->task_count--;
    pthread_mutex_unlock(&pool->mutex);

    task->function(task->arg);
    free(task);
    
    return 1;
}

void destroy_thread_pool(thread_pool_t* pool) {
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);

    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    task_t* task = pool->head;
    while (task) {
        task_t* next = task->next;
        free(task);
        task = next;
    }

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);

    free(pool->threads);
    free(pool);
}