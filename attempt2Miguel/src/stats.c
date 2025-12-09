#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h> 
#include "stats.h"
#include "shared_mem.h"
#include "semaphores.h"

void update_atomic_stats(shared_data_t* shm, semaphores_t* sems, long response_size, int status_code, long response_time){
    sem_wait(sems->stats_mutex);

    shm->stats.total_requests++;
    shm->stats.bytes_transferred += response_size;
    shm->stats.total_response_time += response_time;

    switch (status_code)
    {
    case 200:
        shm->stats.status_200++;
        break;

    case 404: 
        shm->stats.status_404++;
        break;
    
    case 500: 
        shm->stats.status_500++;
        break;
    case 206:
        shm->stats.status_206++;
    default:
        break;
    }

    sem_post(sems->stats_mutex);
}

void update_active_connection(shared_data_t* shm, semaphores_t* sems){
    sem_wait(sems->stats_mutex);
    shm->stats.active_connections++;
    sem_post(sems->stats_mutex);
}

void update_connection_terminated(shared_data_t* shm, semaphores_t* sems){
    sem_wait(sems->stats_mutex);
    if (shm->stats.active_connections > 0)
        shm->stats.active_connections--;
    sem_post(sems->stats_mutex);
}

void print_stats(shared_data_t* shm, semaphores_t* sems) {
    sem_wait(sems->stats_mutex);

    long total = shm->stats.total_requests;
    double avg_response = (total > 0) ? shm->stats.total_response_time / total : 0.0;

    printf("Requests: %ld\n", shm->stats.total_requests);
    printf("Bytes: %ld\n", shm->stats.bytes_transferred);
    printf("Status 200: %ld\n", shm->stats.status_200);
    printf("Status 404: %ld\n", shm->stats.status_404);
    printf("Status 500: %ld\n", shm->stats.status_500);
    printf("Status 206: %ld\n", shm->stats.status_206);
    printf("Active connections: %d\n", shm->stats.active_connections);
    printf("Average response time: %.3f ms\n", avg_response);

    sem_post(sems->stats_mutex);
}