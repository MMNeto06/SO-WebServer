#ifndef HTTP_H 
#define HTTP_H 
#include "config.h" 
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct { 
    char method[16]; 
    char path[512]; 
    char version[16]; 
    int want_close;
    long range_start;
    long range_end;
    int has_range;
} http_request_t; 
 
const char* get_mime_type(const char *filename);
http_request_t* http_parse_request(char *buffer, server_config_t config);
long http_send_503(SSL *ssl);
char* http_build_response(http_request_t *req, int *status_code, long *out_size, server_config_t config);
void http_send_response(SSL *ssl, char *response, long size);

#endif 