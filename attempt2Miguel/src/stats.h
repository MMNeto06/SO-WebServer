#ifndef STATS_H
#define STATS_H

#include "shared_mem.h"
#include "semaphores.h"

void update_atomic_stats(shared_data_t* shm, semaphores_t* sems, long response_size, int status_code, long response_time);
void update_active_connection(shared_data_t* shm, semaphores_t* sems);
void update_connection_terminated(shared_data_t* shm, semaphores_t* sems);
void print_stats(shared_data_t* shm, semaphores_t* sems);

#endif 