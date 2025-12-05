#ifndef HTTP_H 
#define HTTP_H 
#include "config.h" 

typedef struct { 
    char method[16]; 
    char path[512]; 
    char version[16]; 
    int want_close;
} http_request_t; 
 
const char* get_mime_type(const char *filename);
http_request_t* http_parse_request(char *buffer, server_config_t config);
char* http_build_response(http_request_t *req, int *status_code, long *out_size, server_config_t config);
void http_send_response(int fd, char *response, long size);

#endif 