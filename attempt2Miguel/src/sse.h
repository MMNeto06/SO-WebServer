#ifndef SSE_H
#define SSE_H

#include <unistd.h>
#include "shared_mem.h"
#include "semaphores.h"

void handle_sse(int client_fd, shared_data_t *shm, semaphores_t *sems);

#endif 