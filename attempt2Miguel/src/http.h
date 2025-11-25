#ifndef HTTP_H 
#define HTTP_H 
 
typedef struct { 
    char method[16]; 
    char path[512]; 
    char version[16]; 
} http_request_t; 
 
const char* get_mime_type(const char *filename);
http_request_t* http_parse_request(char *buffer);
char* http_build_response(http_request_t *req, int *status_code, size_t *out_size);
void http_send_response(int fd, char *response, size_t size);

#endif 