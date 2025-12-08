#ifndef WORKER_H
#define WORKER_H

#include "shared_mem.h"
#include "semaphores.h"
#include "cache.h"
#include "config.h"
#include "thread_pool.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

#define SHM_NAME "/webserver_shm"

typedef struct worker {
    int pid; 
    int id;         
    int uds_socket;      
    char socket_path[108]; 
    shared_data_t* shm;
    semaphores_t* sems;
    cache_t* cache;
    thread_pool_t* pool;
} worker_t;

typedef struct {
    int client_fd;
    shared_data_t* shm; 
    semaphores_t* sems;
    cache_t* cache; 
    sem_t* log_sem;
    server_config_t config;
    SSL_CTX *ssl_ctx;
    SSL *ssl;
} worker_task_args_t;

void* worker_thread(void *arg);
void worker_process_request(void* arg);
void worker_cleanup(worker_t worker);
void worker_main(worker_t worker, server_config_t config);



#endif