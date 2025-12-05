#ifndef WORKER_H
#define WORKER_H

#include "shared_mem.h"
#include "semaphores.h"
#include "cache.h"
#include "config.h"

#define SHM_NAME "/webserver_shm"

typedef struct worker {
    int id;         
    int uds_socket;       
    shared_data_t* shm;
    semaphores_t* sems;
    cache_t* cache;
} worker_t;

typedef struct {
    int client_fd;
    shared_data_t* shm; 
    semaphores_t* sems;
    cache_t* cache; 
    sem_t* log_sem;
    server_config_t config;
} worker_task_args_t;

void* worker_thread(void *arg);
void worker_process_request(void* arg);
void worker_main(worker_t worker, server_config_t config);



#endif