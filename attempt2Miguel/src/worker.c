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
#include <sys/un.h>
#include <time.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "config.h"
#include "shared_mem.h"
#include "semaphores.h"
#include "thread_pool.h"
#include "worker.h"
#include "http.h"
#include "stats.h"
#include "cache.h"
#include "logger.h"
#include "sse.h"

SSL_CTX *ssl_ctx;

void ssl_init(char* cert, char* key) {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    ssl_ctx = SSL_CTX_new(TLS_server_method());

    SSL_CTX_use_certificate_file(ssl_ctx, cert, SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ssl_ctx, key, SSL_FILETYPE_PEM);
}

char logmsg[4096];

int recv_fd(int uds_sock) {
    struct msghdr msg = {0};
    struct iovec iov;
    char buf[1];  
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char cbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof(cbuf);

    if (recvmsg(uds_sock, &msg, 0) < 0) {
        perror("recvmsg");
        return -1;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == NULL || cmsg->cmsg_type != SCM_RIGHTS) {
        fprintf(stderr, "No FD received\n");
        return -1;
    }

    int client_fd;
    memcpy(&client_fd, CMSG_DATA(cmsg), sizeof(int));
    return client_fd;
}

void worker_process_request(void* arg){
    worker_task_args_t* args = (worker_task_args_t*) arg;
    clock_t start = clock();
    update_active_connection(args->shm, args->sems);

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(args->client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    SSL *ssl = SSL_new(args->ssl_ctx);
    SSL_set_fd(ssl, args->client_fd);

    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(args->client_fd);
        return;
    }
    while (1){
        char buffer[4096];

        //int bytes = recv(args->client_fd, buffer, sizeof(buffer)-1, 0);
        int bytes = SSL_read(ssl, buffer, sizeof(buffer)-1);
        if (bytes <= 0){
            perror("recv");
            log_message(args->sems->log_mutex, "ERROR", "recv error", args->config);
            break; 
        } 
        
        buffer[bytes] = '\0';

        http_request_t* req = http_parse_request(buffer, args->config);

        char client_ip[64] = "unknown";
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        if(getpeername(args->client_fd, (struct sockaddr*)&addr, &len) == 0){
            inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
        }

        cache_entry_t* cached = cache_get(args->cache, req->path);
        if(cached){ //se a request já existir em cache, usar isso diretamente
            printf("Worker hit cache for %s\n", req->path);
            sprintf(logmsg, "Worker hit cache for %s\n", req->path);
            log_message(args->log_sem, "INFO", logmsg, args->config);

            //http_send_response(args->client_fd, cached->data, cached->size);
            http_send_response(ssl, cached->data, cached->size);
            log_request(args->log_sem, client_ip, req->method, req->path, 200, cached->size, args->config);

            update_connection_terminated(args->shm, args->sems);
            close(args->client_fd);
            free(req);
            free(args);
            return;
        }
        printf("DEBUG: %s\n", req->path);

        //se não existir, formular response
        long response_size;
        int status_code; 
        char* response = http_build_response(req, &status_code, &response_size, args->config);
        void* cache_copy;

        if (response_size == -1) {
            log_message(args->log_sem, "DEBUG", "SSE connected", args->config);
            printf("DEBUG: SSE connected.\n");
            handle_sse(ssl, args->shm, args->sems);
            free(req);
            close(args->client_fd);
            free(args);
            return;
        }

        if (response != NULL){
            //guardar response em cache
            cache_copy = malloc(response_size);
            memcpy(cache_copy, response, response_size);
            cache_put(args->cache, req->path, cache_copy, response_size);

            //http_send_response(args->client_fd, response, response_size);
            http_send_response(ssl, response, response_size);
            log_request(args->log_sem, client_ip, req->method, req->path, status_code, response_size, args->config);
            free(response);
        } 
        clock_t end = clock(); 
        long response_time = (long) (end-start) / CLOCKS_PER_SEC;
        update_atomic_stats(args->shm, args->sems, response_size, status_code, response_time);

        if (req->want_close){
            free(req);
            break;
        }

        free(req);
    }
    
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(args->client_fd);
    update_connection_terminated(args->shm, args->sems);
    free(args);
}

void* worker_thread(void* arg) {
    thread_pool_t* pool = (thread_pool_t*)arg;

    while (1) {
        if(thread_pool_dequeue(pool) == 0){
            break;
        }
    }

    return NULL;
}

void worker_main(worker_t worker, server_config_t config){
    printf("Worker ID: %d\n", worker.id);
    sprintf(logmsg, "Worker %d initiated", worker.id);
    log_message(worker.sems->log_mutex, "INFO", logmsg, config);

    /*server_config_t config;
    load_config("../server.conf", &config);*/

    //worker.cache = cache_init(config.cache_size_mb);
    ssl_init(config.certificate, config.key);

    thread_pool_t* pool = create_thread_pool(config.threads_per_worker);

    int uds_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (uds_sock < 0) { perror("socket"); exit(1); }
    worker.uds_socket = uds_sock;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    sprintf(addr.sun_path, "/tmp/worker_socket_%d", worker.id);

    if (connect(uds_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(1);
    }
    printf("Worker %d conectado ao master via UDS\n", worker.id);

    sprintf(logmsg, "Worker %d conectado ao master via UDS", worker.id);
    log_message(worker.sems->log_mutex, "INFO", logmsg, config);

    while (worker.shm->stats.keep_running){
        int client_fd = recv_fd(uds_sock);
        if (client_fd < 0) continue;

        worker_task_args_t* args = malloc(sizeof(worker_task_args_t));
        args->client_fd = client_fd;
        args->shm = worker.shm;
        args->sems = worker.sems;
        args->cache = worker.cache;
        args->log_sem = worker.sems->log_mutex; //hopefully does not break
        args->config = config;
        args->ssl_ctx = ssl_ctx;
        add_task(pool, worker_process_request, args);
    }

    sprintf(logmsg, "Worker %d is being destroyed", worker.id);
    log_message(worker.sems->log_mutex, "INFO", logmsg, config);
    destroy_thread_pool(pool);
    //cache_destroy(worker.cache);
    close(uds_sock);
}

