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
#include "uds.h"
#include "ssl.h"

extern worker_t* workers;
char logmsg[4096];

static void get_client_ip(int fd, char* buf){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if(getpeername(fd, (struct sockaddr*)&addr, &len) == 0){
        inet_ntop(AF_INET, &addr.sin_addr, buf, 64);
    }
}

static void send_cached_response(worker_task_args_t* args, SSL* ssl, http_request_t* req, cache_entry_t* cached, char* client_ip, clock_t start){
    sprintf(logmsg, "Worker hit cache for %s\n", req->path);
    log_message(args->log_sem, "INFO", logmsg, args->config);

    http_send_response(ssl, cached->data, cached->size);
    log_request(args->log_sem, client_ip, req->method, req->path, 200, cached->size, args->config);
    clock_t end = clock();
    double response_time = ((double)(end - start) * 1000.0) / CLOCKS_PER_SEC;
    update_atomic_stats(args->shm, args->sems, cached->size, 200, response_time);
}

static void cache_response(worker_task_args_t* args, const char* path, char* response, long size){
    void* cache_copy = malloc(size);
    if (cache_copy){
        memcpy(cache_copy, response, size);
        cache_put(args->cache, path, cache_copy, size);
    }
}

static void handle_request(worker_task_args_t* args, SSL* ssl, http_request_t* req, clock_t start){
    if (strstr(req->path, ".html") != NULL) {
        update_active_connection(args->shm, args->sems);
    }

    char client_ip[64] = "unknown";
    get_client_ip(args->client_fd, client_ip);

    cache_entry_t* cached = cache_get(args->cache, req->path);
    if (cached){
        send_cached_response(args, ssl, req, cached, client_ip, start);
        return;
    }

    long response_size;
    int status_code;
    char* response = http_build_response(req, &status_code, &response_size, args->config);

    if (response_size == -1){
        handle_sse(ssl, args->shm, args->sems);
        return;
    }

    if (response){
        cache_response(args, req->path, response, response_size);
        http_send_response(ssl, response, response_size);
        log_request(args->log_sem, client_ip, req->method, req->path, status_code, response_size, args->config);
        free(response);
    }

    clock_t end = clock();
    double response_time = ((double)(end - start) * 1000.0) / CLOCKS_PER_SEC;
    update_atomic_stats(args->shm, args->sems, response_size, status_code, response_time);
}

static void process_requests_loop(worker_task_args_t* args, SSL* ssl){
    while (args->shm->stats.keep_running){
        clock_t start = clock();
        char buffer[4096];
        int bytes = SSL_read(ssl, buffer, sizeof(buffer)-1);

        if (handle_ssl_read_errors(bytes, ssl, args)) break;

        buffer[bytes] = '\0';
        http_request_t* req = http_parse_request(buffer, args->config);
        if (!req) {
            log_message(args->sems->log_mutex, "ERROR", "Failed to parse HTTP request", args->config);
            break;
        }

        handle_request(args, ssl, req, start);

        if (req->want_close){
            free(req);
            break;
        }

        free(req);
    }
}

void worker_process_request(void* arg){
    worker_task_args_t* args = (worker_task_args_t*) arg;
    SSL *ssl = ssl_setup(args);
    if (!ssl) return;

    process_requests_loop(args, ssl);

    ssl_teardown(args, ssl);
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

void worker_cleanup(worker_t worker){
    destroy_thread_pool(worker.pool);
    cache_destroy(worker.cache);
    destroy_semaphores(worker.sems);
    free(worker.sems);
    destroy_shared_memory(worker.shm);
    close(worker.uds_socket);
    unlink(worker.socket_path);
}

void worker_main(worker_t worker, server_config_t config){
    sprintf(logmsg, "Worker %d initiated", worker.id);
    log_message(worker.sems->log_mutex, "INFO", logmsg, config);

    if (workers != NULL){
        free(workers);
        workers = NULL;
    }

    SSL_CTX *ssl_ctx = ssl_init(config.certificate, config.key);

    worker.pool = create_thread_pool(config.threads_per_worker, config.max_queue_size);

    int uds_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (uds_sock < 0) { 
        log_message(worker.sems->log_mutex, "WARN", "Invalid UDS Socket, exiting...", config); 
        exit(1); 
    }
    worker.uds_socket = uds_sock;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    sprintf(addr.sun_path, "/tmp/worker_socket_%d", worker.id);

    if (connect(uds_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_message(worker.sems->log_mutex, "WARN", "connect failed", config);
        exit(1);
    }

    sprintf(logmsg, "Worker %d conectado ao master via UDS", worker.id);
    log_message(worker.sems->log_mutex, "INFO", logmsg, config);

    while (worker.shm->stats.keep_running){
        int client_fd = recv_fd(uds_sock, worker.sems, config);
        if (client_fd < 0) continue;

        worker_task_args_t* args = malloc(sizeof(worker_task_args_t));
        args->client_fd = client_fd;
        args->shm = worker.shm;
        args->sems = worker.sems;
        args->cache = worker.cache;
        args->log_sem = worker.sems->log_mutex; 
        args->config = config;
        args->ssl_ctx = ssl_ctx;
        if (add_task(worker.pool, worker_process_request, args) != 0){
            log_message(worker.sems->log_mutex, "WARN", "Queue is full. Handling 503 error.", config);
            SSL *ssl = SSL_new(args->ssl_ctx);
            if (ssl) {
                SSL_set_fd(ssl, client_fd);

                if (SSL_accept(ssl) > 0) {
                    long response_size = http_send_503(ssl);
                    int status_code = 503;
                    update_atomic_stats(worker.shm, worker.sems, response_size, status_code, 0); // 0 = response time, não iremos contabilizar 
                    
                    SSL_shutdown(ssl);
                }

                SSL_free(ssl);
            }
            free(args); 
            continue;
        }
    }

    sprintf(logmsg, "Worker %d is being destroyed", worker.id);
    log_message(worker.sems->log_mutex, "INFO", logmsg, config);
    worker_cleanup(worker);
    SSL_CTX_free(ssl_ctx);
    ssl_ctx = NULL;

}

