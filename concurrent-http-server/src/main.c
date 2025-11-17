#include <stdio.h> 
#include <string.h> 
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include "config.h"
#include "master.h"

int main() { 
    server_config_t config;
    load_config("../server.conf", &config); //DÁ RUN DENTRO DE SRC SE NÃO QUERES ALTERAR ISTO

    int server_fd = create_server_socket(config.port); //está no master.c, inicializa a socket, pode não ficar aqui? idk
 
    printf("HTTP server running on http://localhost:%d\n", config.port); 

    shared_data_t* shm = create_shared_memory();
    if (!shm) {
        perror("Create Shared Memory");
        return 1;
    }

    semaphores_t sems;
    if (init_semaphores(&sems, config.max_queue_size) != 0) {
        perror("init_semaphores");
        return 1;
    }

    create_workers(config.num_workers, server_fd, shm, &sems);
 
    master_loop(server_fd, shm, &sems);

    return 0; 
}