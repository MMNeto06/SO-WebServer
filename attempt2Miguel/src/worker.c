#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <signal.h> 
#include <pthread.h>

#include "config.h"
#include "shared_mem.h"
#include "semaphores.h"
#include "thread_pool.h"

#define SHM_NAME "/webserver_shm"   

/*void* handle_client(void* arg) { 
    printf("handle_client called on thread %lu\n", pthread_self());
    int client_fd = *(int*)arg; 
    free(arg); 
 
    // Read request, send response 
    char buffer[4096]; 
    recv(client_fd, buffer, sizeof(buffer), 0); 
     
    const char* response = "HTTP/1.1 200 OK\r\n\r\n<h1>Hello</h1>"; 
    send(client_fd, response, strlen(response), 0); 
     
    close(client_fd); 
    return NULL; 
} */

void send_file(void* arg) { //, const char* path
    // Open file 
    printf("send_file called on thread %lu\n", pthread_self());
    int client_fd = *(int*)arg; 
    free(arg); 

    server_config_t config;
    load_config("../server.conf", &config);

    
    const char* path = strcat(config.document_root, "/index.html"); //talvez é suposto buscar isto ao config file, na variável document root, não sei
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
    close(client_fd);
    return;
}

void worker(int *client_fd){ //copiado de threaded server
    server_config_t config;
    load_config("../server.conf", &config);

    static thread_pool_t *pool = NULL;

    if (pool == NULL) {
        pool = create_thread_pool(config.threads_per_worker);
    }

    int *fd_copy = malloc(sizeof(int));
    *fd_copy = *client_fd;

    add_task(pool, (void*)send_file, fd_copy);
}