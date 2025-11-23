#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <signal.h> 
#include <pthread.h>

#include "config.h"
#include "shared_mem.h"
#include "semaphores.h"
#include "worker.h"
 
volatile sig_atomic_t keep_running = 1; 
semaphores_t* sems; 
shared_data_t* shm;
 
void master_signal_handler(int signum) { 
    printf("Signum: %d", signum);
    keep_running = 0; 
    shm->stats.keep_running = 0;
} 
 
int create_server_socket(int port) { 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); 

    if (sockfd < 0){
        perror("socket");
        return -1; 
    }

    int opt = 1; 
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 
     
    struct sockaddr_in addr; 
    addr.sin_family = AF_INET; 
    addr.sin_addr.s_addr = INADDR_ANY; 
    addr.sin_port = htons(port); 
     
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { 
        close(sockfd); 
        return -1; 
    } 
     
    if (listen(sockfd, 128) < 0) { 
        close(sockfd); 
        return -1; 
    } 
     
    return sockfd; 
} 
 
void enqueue_connection(shared_data_t* data, semaphores_t* sems, int client_fd) { 
    sem_wait(sems->empty_slots); 
    sem_wait(sems->queue_mutex); 
     
    data->queue.sockets[data->queue.rear] = client_fd; 
    data->queue.rear = (data->queue.rear + 1) % MAX_QUEUE_SIZE; 
    data->queue.count++; 
     
    sem_post(sems->queue_mutex); 
    sem_post(sems->filled_slots); 

}

void create_workers(int num_workers){
    int i;

    worker_t* workers = malloc(sizeof(worker_t)*num_workers);
    for (i = 0; i < num_workers; i++) {
        workers[i].id = i;
        workers[i].shm = shm;
        workers[i].sems = sems;
    }

    for (i = 0; i < num_workers; i++){
        switch (fork())
        {
        case -1:
            perror("fork");
            continue;
        
        case 0: 
            worker_main(workers[i]);
            exit(0);

        default:
            break;
        }
    }

    free(workers);
}

void master_cleanup(void){
    destroy_semaphores(sems);
    destroy_shared_memory(shm);
}

int master_init(server_config_t config){
    int server_fd = create_server_socket(config.port); //está no master.c, inicializa a socket, pode não ficar aqui? idk
    
    if (server_fd < 0) {
        perror("create_server_socket");
        return -1;
    }
 
    printf("HTTP server running on http://localhost:%d\n", config.port); 

    shm = create_shared_memory();
    if (shm == NULL){
        perror("create_shared_memory");
        return -1;
    }
    shm->stats.keep_running = 1;

    sems = malloc(sizeof(semaphores_t));
    if (init_semaphores(sems, config.max_queue_size) != 0){
        perror("init_semaphores");
        return -1;
    }

    return server_fd;
}

void master_accept_loop(int listen_fd){
    server_config_t config;
    load_config("../server.conf", &config);
    while (keep_running) { 
        int client_fd = accept(listen_fd, NULL, NULL); 
        printf("DEBUG: Accepted client_fd = %d\n", client_fd);
        if (client_fd < 0){
            continue;
        }
        enqueue_connection(shm, sems, client_fd);
        create_workers(config.num_workers);
    }  

    master_cleanup();
    if (listen_fd >= 0) close(listen_fd);
}

int main(void){
    struct sigaction sigact;

    /* altera a rotina de atendimento ao ^C */
    sigact.sa_handler = master_signal_handler;
    sigemptyset (&sigact.sa_mask);
    sigact.sa_flags = 0;
    if (sigaction(SIGINT, &sigact, NULL) < 0) { 
        perror("Rotina de atendimento não instalada\n");
        return EXIT_FAILURE;
    }

    /* altera a rotina de atendimento ao ^Z */
    sigact.sa_handler = master_signal_handler;
    sigemptyset (&sigact.sa_mask);
    sigact.sa_flags = 0;
    if (sigaction(SIGTERM, &sigact, NULL) < 0) { 
        perror("Rotina de atendimento não instalada\n");
        return EXIT_FAILURE;
    }

    server_config_t config;
    load_config("../server.conf", &config); //DÁ RUN DENTRO DE SRC SE NÃO QUERES ALTERAR ISTO
    int server_fd = master_init(config);
    if (server_fd < 0){
        perror("master_init");
        exit(1);
    }
    master_accept_loop(server_fd);

    return 0; 
}