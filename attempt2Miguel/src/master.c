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
 
void signal_handler(int signum) { 
    printf("Signum: %d", signum);
    keep_running = 0; 
} 
 
int create_server_socket(int port) { 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd < 0) return -1; 
     
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

int main(void){
    server_config_t config;
    load_config("../server.conf", &config); //DÁ RUN DENTRO DE SRC SE NÃO QUERES ALTERAR ISTO
    
    int server_fd = create_server_socket(config.port); //está no master.c, inicializa a socket, pode não ficar aqui? idk
    
    if (server_fd < 0) {
        perror("create_server_socket");
        return 1;
    }
 
    printf("HTTP server running on http://localhost:%d\n", config.port); 

    while (keep_running) { 
        int* client_fd = malloc(sizeof(int)); 
        *client_fd = accept(server_fd, NULL, NULL); 
        
        worker(client_fd);
    } 
    return 0; 

    close(server_fd);

    return 0; 
}