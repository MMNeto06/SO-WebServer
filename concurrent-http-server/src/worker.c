#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "shared_mem.h"
#include "semaphores.h"

#define SHM_NAME "/webserver_shm"

shared_data_t* open_shared_memory(){
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);

    if (shm_fd == -1){
        perror("shm_open worker");
        exit(1);
    }

    shared_data_t* data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (data == MAP_FAILED){
        perror("mmap worker");
        exit(1);
    }

    return data;    
}

semaphores_t* open_semaphores() {
    semaphores_t* sems = malloc(sizeof(semaphores_t));

    if (sems == NULL){
        perror("malloc");
        exit(1);    
    }

    sems->empty_slots  = sem_open("/ws_empty", 0);
    sems->filled_slots = sem_open("/ws_filled", 0);
    sems->queue_mutex  = sem_open("/ws_queue_mutex", 0);
    sems->stats_mutex  = sem_open("/ws_stats_mutex", 0);
    sems->log_mutex    = sem_open("/ws_log_mutex", 0);

    if (sems->empty_slots == SEM_FAILED ||
        sems->filled_slots == SEM_FAILED ||
        sems->queue_mutex == SEM_FAILED ||
        sems->stats_mutex == SEM_FAILED ||
        sems->log_mutex == SEM_FAILED) {

        perror("sem_open worker");
        exit(1);
    }

    return sems;
}

void worker_loop(int worker_id, int server_fd) {
    shared_data_t* shm = open_shared_memory(); 
    semaphores_t* sems = open_semaphores(); 

    printf("[worker %d] iniciado! total_requests=%d\n", worker_id, shm->stats.total_requests);

    while (1) {
        sem_wait(sems->stats_mutex);

        shm->stats.total_requests++;
        printf("[worker %d] iniciado! total_requests=%d\n", worker_id, shm->stats.total_requests);

        sem_post(sems->stats_mutex);

        sleep(1);
    }
}