#ifndef SSE_H
#define SSE_H

#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "shared_mem.h"
#include "semaphores.h"

void handle_sse(SSL *ssl, shared_data_t *shm, semaphores_t *sems);

#endif 