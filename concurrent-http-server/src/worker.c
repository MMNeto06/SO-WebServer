#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <signal.h> 

#include "shared_mem.h"
#include "semaphores.h"

#define SHM_NAME "/webserver_shm"   

/*const char* response =  
    "HTTP/1.1 200 OK\r\n" 
    "Content-Type: text/html\r\n" 
    "\r\n" 
    "<html><body><h1>Hello, World!</h1></body></html>"; */

void send_file(int client_fd, const char* path) { //esta função enviat html, está no pdf 05
    // Open file 
    FILE* file = fopen(path, "rb"); 
    /*if (!file) { 
        const char* not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 22\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<h1>404 Not Found</h1>";
        send(client_fd, not_found, strlen(not_found), 0); 
        return; 
    } */
 
    printf("DEBUG: Tou no send_file?\n");

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

void worker_loop(int worker_id) {
    shared_data_t* shm = open_shared_memory(); 
    semaphores_t* sems = open_semaphores(); 

    while (1) {
        printf("[WORKER %d] Esperando semáforo...\n", worker_id);
        sem_wait(sems->filled_slots);
        printf("[WORKER %d] Acordei!\n", worker_id);
        sem_wait(sems->queue_mutex);

        int client_fd = shm->queue.sockets[shm->queue.front];
        shm->queue.front = (shm->queue.front + 1) % MAX_QUEUE_SIZE;
        shm->queue.count--;

        printf("[worker %d] got fd=%d\n", worker_id, client_fd);

        sem_post(sems->queue_mutex);
        sem_post(sems->empty_slots);
        printf("DEBUG: TOU AQUI\n");

        //char buffer[4096];
        //recv(client_fd, buffer, sizeof(buffer), 0);  
        printf("DEBUG: Ainda estou aqui?\n");      
        send_file(client_fd, "../tests/index.html"); //DÁ RUN DENTRO DE SRC SE NÃO QUERES ALTERAR ISTO
        //send(client_fd, response, strlen(response), MSG_DONTWAIT); //usa isto se não quiseres o index.html
        printf("DEBUG: Devia ter dado load?\n");
        close(client_fd); 
    }
}