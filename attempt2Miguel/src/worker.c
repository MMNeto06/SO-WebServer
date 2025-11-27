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

#include "config.h"
#include "shared_mem.h"
#include "semaphores.h"
#include "thread_pool.h"
#include "worker.h"
#include "http.h"
#include "stats.h"

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
    char buffer[4096];

    int bytes = recv(args->client_fd, buffer, sizeof(buffer)-1, 0);
    if (bytes <= 0){
        perror("recv");
        return; 
    }

    buffer[bytes] = '\0';

    update_active_connection(args->shm, args->sems);

    http_request_t* req = http_parse_request(buffer);

    long response_size;
    int status_code; 
    char* response = http_build_response(req, &status_code, &response_size);

    if (response != NULL)
        http_send_response(args->client_fd, response, response_size);

    clock_t end = clock(); 
    long response_time = (long) (end-start) / CLOCKS_PER_SEC;
    update_atomic_stats(args->shm, args->sems, response_size, status_code, response_time);
    
    update_connection_terminated(args->shm, args->sems);
    close(args->client_fd);
    free(response);
    free(req);
    free(args);
}

void* worker_thread(void* arg) {
    thread_pool_t* pool = (thread_pool_t*)arg;

    while (1) {
        pthread_mutex_lock(&pool->mutex);

        while (!pool->shutdown && pool->head == NULL) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        task_t* task = pool->head;
        pool->head = task->next;
        if (pool->head == NULL) pool->tail = NULL;

        pthread_mutex_unlock(&pool->mutex);

        task->function(task->arg);
        free(task);
    }

    return NULL;
}

void worker_main(worker_t worker){
    printf("Worker ID: %d\n", worker.id);

    server_config_t config;
    load_config("../server.conf", &config);

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

    while (worker.shm->stats.keep_running){
        int client_fd = recv_fd(uds_sock);
        if (client_fd < 0) continue;

        worker_task_args_t* args = malloc(sizeof(worker_task_args_t));
        args->client_fd = client_fd;
        args->shm = worker.shm;
        args->sems = worker.sems;

        add_task(pool, worker_process_request, args);
    }

    destroy_thread_pool(pool);
    close(uds_sock);
}

