#include <stdio.h>
#include <string.h>
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <signal.h> 
#include <stdlib.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
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

http_request_t* http_parse_request(char *buffer, server_config_t config) { 
    http_request_t* http_request = malloc(sizeof(http_request_t));
    /*server_config_t config;
    load_config("../server.conf", &config);*/

    sscanf(buffer, "%s %s %s", http_request->method, http_request->path, http_request->version);
    if (strcmp(http_request->path, "/") == 0){
        snprintf(http_request->path, sizeof(http_request->path), "%s/index.html", config.document_root);
    } else {
        char temp_path[strlen(http_request->path) + 512];
        snprintf(temp_path, sizeof(temp_path), "%s%s", config.document_root, http_request->path);
        strcpy(http_request->path, temp_path);
    }

    http_request->want_close = 0; // default HTTP/1.1
    http_request->has_range = 0;
    http_request->range_start = 0;
    http_request->range_end = -1;

    // HTTP/1.0 força close
    if (strncmp(http_request->version, "HTTP/1.0", 8) == 0) {
        http_request->want_close = 1;
    }

    char *line = strstr(buffer, "\r\n") + 2;  // first header line

    while (line && *line != '\r') {
        char key[64] = {0};
        char value[256] = {0};

        sscanf(line, "%63[^:]: %255[^\r\n]", key, value);

        if (key[0] != '\0') {
            if (strcasecmp(key, "Connection") == 0) {
                if (strcasecmp(value, "close") == 0)
                    http_request->want_close = 1;
                if (strcasecmp(value, "keep-alive") == 0)
                    http_request->want_close = 0;
            }

            if (strcasecmp(key, "Range") == 0) {
                long start = 0, end = -1;

                char *eq = strchr(value, '=');
                if (eq) {
                    sscanf(eq + 1, "%ld-%ld", &start, &end);
                    http_request->range_start = start;
                    http_request->range_end = end;
                    http_request->has_range = 1;
                }
            }
        }

        // move to next header line
        line = strstr(line, "\r\n");
        if (line)
            line += 2;
    }
        
    return http_request;
} 

char* http_build_response(http_request_t *req, int *status_code, long *out_size, server_config_t config) { 
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
    char *full_stats_path = malloc(strlen(config.document_root) + strlen("/stats") + 1);
    sprintf(full_stats_path, "%s/stats", config.document_root);
    if (strcmp(req->path, full_stats_path) == 0) {
        printf("DEBUG: Stats detected!\n");
        *status_code = 200;
        *out_size = -1; // flag SSE
        free(full_stats_path);
        return NULL;
    }

    free(full_stats_path);
    
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
        
        if (req->has_range) {
            
            long start = req->range_start;
            long end   = (req->range_end == -1) ? (f_size - 1) : req->range_end;

            if (start < 0) start = 0;
            if (end >= f_size) end = f_size - 1;
            if (start > end) start = end;

            long chunk_size = end - start + 1;

            char header[512];
            sprintf(header,
                "HTTP/1.1 206 Partial Content\r\n"
                "%s"
                "Content-Type: %s\r\n"
                "Content-Length: %ld\r\n"
                "Content-Range: bytes %ld-%ld/%ld\r\n\r\n",
                connection_header,
                mime_type,
                chunk_size,
                start, end, f_size
            );

            *out_size = strlen(header) + chunk_size;

            char* response = malloc(*out_size + 1);
            memcpy(response, header, strlen(header));
            memcpy(response + strlen(header), f_data + start, chunk_size);

            free(f_data);

            *status_code = 206;
            return response;
        }

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
        // 500 aqui     
        return NULL; 
    }
}

void http_send_response(SSL* ssl, char *response, long size){
    long sent = 0;
    while (sent < size){
        //long n = send(fd, response, size, 0);
        long n = SSL_write(ssl, response + sent, size - sent);
        if (n<=0){
            perror("send");
            return;
        }
        sent += n;
    }
        
}