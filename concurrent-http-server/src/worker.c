#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <sys/socket.h> 
#include <netinet/in.h> 

#include "shared_mem.h"
#include "semaphores.h"

#define SHM_NAME "/webserver_shm"   

/*const char* response =  
    "HTTP/1.1 200 OK\r\n" 
    "Content-Type: text/html\r\n" 
    "\r\n" 
    "<html><body><h1>Hello, World!</h1></body></html>";*/ 

void send_file(int client_fd, const char* path) { //esta função enviat html, está no pdf 05
    // Open file 
    FILE* file = fopen(path, "rb"); 
    if (!file) { 
        // Send 404 
        const char* response = 
            "HTTP/1.1 404 Not Found\r\n" 
            "Content-Type: text/html\r\n" 
            "\r\n" 
            "<h1>404 Not Found</h1>"; 
        send(client_fd, response, strlen(response), 0); 
        return; 
    } 
 
    // Get file size 
    fseek(file, 0, SEEK_END); 
    long file_size = ftell(file); 
    fseek(file, 0, SEEK_SET); 
 
    // Send headers 
    char header[512]; 
    snprintf(header, sizeof(header), 
        "HTTP/1.1 200 OK\r\n" 
        "Content-Type: text/html\r\n" 
        "Content-Length: %ld\r\n" 
        "\r\n", file_size); 
    send(client_fd, header, strlen(header), 0); 
 
    // Send file content 
    char buffer[4096]; 
    size_t bytes; 
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) { 
        send(client_fd, buffer, bytes, 0); 
    }
    fclose(file); 
}


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

void worker_loop(int worker_id) {
    shared_data_t* shm = open_shared_memory(); 
    semaphores_t* sems = open_semaphores(); 

    while (1) {
        sem_wait(sems->filled_slots);
        sem_wait(sems->queue_mutex);

        int client_fd = shm->queue.sockets[shm->queue.front];
        shm->queue.front = (shm->queue.front + 1) % MAX_QUEUE_SIZE;
        shm->queue.count--;

        sem_post(sems->queue_mutex);
        sem_post(sems->empty_slots);

        char buffer[4096]; 
        recv(client_fd, buffer, sizeof(buffer), 0);   
        send_file(client_fd, "../tests/index.html"); //DÁ RUN DENTRO DE SRC SE NÃO QUERES ALTERAR ISTO
        //send(client_fd, response, strlen(response), 0); //usa isto se não quiseres o index.html
        close(client_fd); 
    }
}