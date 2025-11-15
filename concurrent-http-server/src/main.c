#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <signal.h> 
#include "master.h"
#include "config.h"

void send_file(int client_fd, const char* path) { 
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

int main(){
    server_config_t config;
    load_config("/concurrent-http-server/server.conf", &config);
    printf("Load config passed");
    int server_fd = create_server_socket(config.port);
    printf("A correr em http://localhost:8080");
    while (1) { 
        int client_fd = accept(server_fd, NULL, NULL); 
         
        char buffer[4096]; 
        recv(client_fd, buffer, sizeof(buffer), 0); 
         
        send_file(client_fd, config.document_root);
        close(client_fd); 
    } 
    return 0; 
}