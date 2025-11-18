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
    int sent, sendCounter; 
    FILE* file = fopen(path, "rb");
    if (!file) {
        // Send 404
        const char* response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<h1>404 Not Found</h1>";
        
        if (send(client_fd, response, strlen(response), 0) <= 0){
            perror("send");
        }
        
        return;
    }
 
    printf("DEBUG: Tou no send_file?\n");

    // Get file size 
    fseek(file, 0, SEEK_END); 
    long file_size = ftell(file); 
    printf("DEBUG: file_size = %ld\n", file_size);
    fseek(file, 0, SEEK_SET); 
 
    sendCounter = 0; 

    // Send headers 
    char header[512]; 
    snprintf(header, sizeof(header), 
        "HTTP/1.1 200 OK\r\n" 
        "Content-Type: text/html\r\n" 
        "Content-Length: %ld\r\n" 
        "\r\n", file_size); 
    if (send(client_fd, header, strlen(header), 0) <= 0){
        printf("DEBUG: sendCounter = %d\n", sendCounter++);
        perror("send");
        return;
    } 
    printf("DEBUG: Mandei o header?\n");
 
    // Send file content 
    char buffer[4096]; 
    size_t bytes; 
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) { 
        sent = send(client_fd, buffer, bytes, 0); 
        printf("sendCounter = %d\n", sendCounter++);
        printf("DEBUG: sent = %d\n", sent);
    }

    if (sent < 0){
        perror("send");
        printf("DEBUG: client_fd = %d\n", client_fd);
        return;
    }

    printf("%d\n", sent);

    printf("DEBUG: Mandei tudo?\n");
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


        printf("DEBUG: Ainda estou aqui?\n");  
        /*char buffer[4096];
        int recv_val = recv(client_fd, buffer, sizeof(buffer), 0); 
        printf("DEBUG: recv_val = %d\n", recv_val);
        if (recv_val <= 0){    
            perror("recv");
            close(client_fd);
            continue;
        } */   
        send_file(client_fd, "../tests/index.html"); //DÁ RUN DENTRO DE SRC SE NÃO QUERES ALTERAR ISTO

        //send(client_fd, response, strlen(response), MSG_DONTWAIT); //usa isto se não quiseres o index.html
        printf("DEBUG: Devia ter dado load?\n");
        close(client_fd); 
    }
}