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

void send_file(void* arg);
int dequeue_connection(shared_data_t* shm, semaphores_t* sems);
void* worker_thread(void *arg);
void worker_process_request(int client_fd);
void worker_main(worker_t worker);



#endif