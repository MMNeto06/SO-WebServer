#ifndef MASTER_H
#define MASTER_H

#include "semaphores.h"
#include "shared_mem.h"

void signal_handler(int signum);
int create_server_socket(int port);
void enqueue_connection(shared_data_t* data, semaphores_t* sems, int client_fd);
void create_workers(int num_workers, int server_fd);
void master_loop(int server_fd, shared_data_t* shm, semaphores_t* sems);

#endif