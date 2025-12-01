#include <stdio.h>
#include <string.h>
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <signal.h> 
#include <stdlib.h>
#include "http.h"
#include "config.h"

const char* get_mime_type(const char *filename) {
    if (strstr(filename, ".html")) return "text/html";
    if (strstr(filename, ".css"))  return "text/css";
    if (strstr(filename, ".js"))   return "application/javascript";
    if (strstr(filename, ".png"))  return "image/png";
    if (strstr(filename, ".jpg"))  return "image/jpeg";
    if (strstr(filename, ".jpeg")) return "image/jpeg";
    if (strstr(filename, ".pdf"))  return "application/pdf";

    return "application/octet-stream";
}

int file_exists(const char *path){
    FILE* file = fopen(path, "rb");
    if (file == NULL){
        return 0; 
    }

    fclose(file);

    return 1;
}

long file_size(const char *path){
    FILE* file = fopen(path, "rb");
    if (!file) return -1;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);
    return size;
}

char* read_file(const char *path){
    FILE* file = fopen(path, "rb");
    long f_size = file_size(path);
    if (f_size <= 0){
        return NULL;
    }

    char* buffer = malloc(f_size);
    fread(buffer, 1, f_size, file);
    fclose(file);
    
    return buffer;
}

http_request_t* http_parse_request(char *buffer) { 
    http_request_t* http_request = malloc(sizeof(http_request_t));
    server_config_t config;
    load_config("../server.conf", &config);

    sscanf(buffer, "%s %s %s", http_request->method, http_request->path, http_request->version);
    if (strcmp(http_request->path, "/") == 0){
        snprintf(http_request->path, sizeof(http_request->path), "%s/index.html", config.document_root);
    } else {
        char temp_path[strlen(http_request->path) + 512];
        snprintf(temp_path, sizeof(temp_path), "%s%s", config.document_root, http_request->path);
        strcpy(http_request->path, temp_path);
    }

    http_request->want_close = 0; // default HTTP/1.1

    // HTTP/1.0 força close
    if (strncmp(http_request->version, "HTTP/1.0", 8) == 0) {
        http_request->want_close = 1;
    }

    char *header_line = strstr(buffer, "\r\n");
    while (header_line && *(header_line += 2) != '\r') {
        char key[64], value[256];
        sscanf(header_line, "%63[^:]: %255[^\r\n]", key, value);

        if (strcasecmp(key, "Connection") == 0) {
            if (strcasecmp(value, "close") == 0)
                http_request->want_close = 1;
            if (strcasecmp(value, "keep-alive") == 0)
                http_request->want_close = 0;
        }
    }
        
    return http_request;
} 

char* http_build_response(http_request_t *req, int *status_code, long *out_size) { 
    server_config_t config;
    load_config("../server.conf", &config);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s", req->path);

    char connection_header[128];
    if (req->want_close){
        sprintf(connection_header, "Connection: close\r\n");
    } else {
        sprintf(connection_header, "Connection: keep-alive\r\n" "Keep-Alive: timeout=5, max=100\r\n");
    }

    char *response; 
    if (strcmp(req->method, "HEAD") == 0) {
        *status_code = 200;
        response = malloc(256);
        sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "%s"
            "Content-Type: text/plain\r\n"
            "Content-Length: 0\r\n\r\n", 
            connection_header
        );
        *out_size = strlen(response);
        return response;
    }

    if (strcmp(req->method, "GET") == 0){
        if (file_exists(filepath) != 1){
            *status_code = 404; 
            response = malloc(256);
            sprintf(response,
                "HTTP/1.1 404 Not Found\r\n"
                "%s"
                "Content-Type: text/plain\r\n"
                "Content-Length: 13\r\n\r\n404 Not Found", 
                connection_header
            );
            *out_size = strlen(response);
            return response; 
        }

        long f_size = file_size(filepath);
        char* f_data = read_file(filepath);
        
        const char* mime_type = get_mime_type(filepath);

        char header[512];
        sprintf(header,
            "HTTP/1.1 200 OK\r\n"
            "%s"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n\r\n",
            connection_header, mime_type, f_size
        );
        *out_size = strlen(header) + f_size;
        response = malloc(*out_size+1);
        memcpy(response, header, strlen(header));
        memcpy(response + strlen(header), f_data, f_size);

        free(f_data);
        *status_code = 200; 

        return response;
    } else {
        printf("ERROR: Invalid Method!\n");
        return NULL; 
    }
}

void http_send_response(int fd, char *response, long size){
    long sent = 0;
    while (sent < size){
        long n = send(fd, response, size, 0);
        if (n<=0){
            perror("send");
            return;
        }
        sent += n;
    }
        
}