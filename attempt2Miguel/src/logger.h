#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "semaphores.h"
#include "config.h"

void log_request(sem_t* log_sem, const char* client_ip, const char* method, const char* path, int status, size_t bytes, server_config_t config);
void log_message(sem_t* log_sem, char* log_level, char* message, server_config_t config);
