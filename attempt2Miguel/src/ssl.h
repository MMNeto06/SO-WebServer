#ifndef SSL_H
#define SSL_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h> 
#include <sys/socket.h>
#include "worker.h"

SSL_CTX* ssl_init(char* cert, char* key);
SSL* ssl_setup(worker_task_args_t* args);
int handle_ssl_read_errors(int bytes, SSL* ssl, worker_task_args_t* args);
void ssl_teardown(worker_task_args_t* args, SSL* ssl);

#endif