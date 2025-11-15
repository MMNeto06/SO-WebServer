//este ficheiro é meramente para testes, não deve ser entregue no fim
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/stat.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
 
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
    int server_fd = socket(AF_INET, SOCK_STREAM, 0); 
    int opt = 1; 
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 
 
    struct sockaddr_in addr = {0}; 
    addr.sin_family = AF_INET; 
    addr.sin_addr.s_addr = INADDR_ANY; 
    addr.sin_port = htons(8080);
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)); 
    listen(server_fd, 10); 
 
    printf("HTTP server running on http://localhost:8080\n"); 
 
    while (1) { 
        int client_fd = accept(server_fd, NULL, NULL); 
         
        char buffer[4096]; 
        recv(client_fd, buffer, sizeof(buffer), 0); 
         
        send_file(client_fd, "index.html");
        close(client_fd); 
    } 
    return 0; 
}