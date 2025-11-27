#ifndef WORKER_H
#define WORKER_H

#include "shared_mem.h"
#include "semaphores.h"

#define SHM_NAME "/webserver_shm"

typedef struct worker {
    int id;         
    int uds_socket;       
    shared_data_t* shm;
    semaphores_t* sems;
} worker_t;

typedef struct {
    int client_fd;
    shared_data_t* shm; 
    semaphores_t* sems;
} worker_task_args_t;

void* worker_thread(void *arg);
void worker_process_request(void* arg);
void worker_main(worker_t worker);



#endif