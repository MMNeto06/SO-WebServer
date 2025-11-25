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

#include "config.h"
#include "shared_mem.h"
#include "semaphores.h"
#include "thread_pool.h"
#include "worker.h"
#include "http.h"

#define SHM_NAME "/webserver_shm"   

/*void* handle_client(void* arg) { 
    printf("handle_client called on thread %lu\n", pthread_self());
    int client_fd = *(int*)arg; 
    free(arg); 
 
    // Read request, send response 
    char buffer[4096]; 
    recv(client_fd, buffer, sizeof(buffer), 0); 
     
    const char* response = "HTTP/1.1 200 OK\r\n\r\n<h1>Hello</h1>"; 
    send(client_fd, response, strlen(response), 0); 
     
    close(client_fd); 
    return NULL; 
} */

void send_file(void* arg) { //, const char* path
    // Open file 
    printf("send_file called on thread %lu\n", pthread_self());
    fflush(stdout);
    int client_fd = *(int*)arg; 
    printf("DEBUG: client_fd = %d\n", client_fd);
    free(arg); 

    server_config_t config;
    load_config("../server.conf", &config);

    const char* path = strcat(config.document_root, "/index.html"); //talvez é suposto buscar isto ao config file, na variável document root, não sei
    FILE* file = fopen(path, "rb"); 
    if (!file) { 
        // Send 404 
        printf("DEBUG: 404?\n");
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
    printf("DEBUG: File size: %lu\n", file_size);
    fseek(file, 0, SEEK_SET); 

    printf("DEBUG: Antes do header!\n");
 
    // Send headers 
    char header[512]; 
    snprintf(header, sizeof(header), 
        "HTTP/1.1 200 OK\r\n" 
        "Content-Type: text/html\r\n" 
        "Content-Length: %ld\r\n" 
        "\r\n", file_size); 
    if (send(client_fd, header, strlen(header), 0) == -1){
        perror("send");
        return;
    }

    printf("DEBUG: Header sent?\n");
 
    // Send file content 
    char buffer[4096]; 
    size_t bytes; 
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) { 
        if (send(client_fd, buffer, bytes, 0) == -1){
            perror("send");
            return;
        }
    }
    printf("DEBUG: Cheguei aqui?\n");
    fclose(file); 
    close(client_fd);
    return;
}

int dequeue_connection(shared_data_t* shm, semaphores_t* sems){
    sem_wait(sems->filled_slots);
    sem_wait(sems->queue_mutex);

    int client_fd = shm->queue.sockets[shm->queue.front];
    shm->queue.front = (shm->queue.front + 1) % MAX_QUEUE_SIZE;
    shm->queue.count--;

    sem_post(sems->queue_mutex);
    sem_post(sems->empty_slots);

    return client_fd;
}

int recv_fd(int uds_sock) {
    struct msghdr msg = {0};
    struct iovec iov;
    char buf[1];  // byte dummy
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


void worker_process_request(int client_fd){
    char buffer[4096];

    int bytes = recv(client_fd, buffer, sizeof(buffer)-1, 0);
    if (bytes <= 0){
        perror("recv");
        return; 
    }

    buffer[bytes] = '\0';

    http_request_t* req = http_parse_request(buffer);

    size_t response_size;
    int status_code; 
    char* response = http_build_response(req, &status_code, &response_size);

    if (response != NULL)
        http_send_response(client_fd, response, response_size);
    
    free(response);
    free(req);
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

    // Thread pool
    thread_pool_t* pool = create_thread_pool(config.threads_per_worker);

    // Socket UDS para receber FDs do master
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

    // Loop principal: receber FDs e adicionar tasks
    while (worker.shm->stats.keep_running){
        //dequeue_connection(worker.shm, worker.sems);
        int client_fd = recv_fd(uds_sock);
        if (client_fd < 0) continue;

        int* fd_copy = malloc(sizeof(int));
        *fd_copy = client_fd;

        add_task(pool, (void*)send_file, fd_copy);
    }

    destroy_thread_pool(pool);
    close(uds_sock);
}


/*void worker_main(int worker_id){ 
    while (1){
        printf("worker%d", worker_id);
        server_config_t config;
        load_config("../server.conf", &config);

        shared_data_t* shm = open_shared_memory();
        if (shm == NULL){
            perror("open_shared_memory");
            return;
        }

        semaphores_t* sems = open_semaphores();
        if (sems == NULL){
            perror("open_semaphores");
            return;
        }

        //sem_wait(sems->filled_slots);
        //sem_wait(sems->queue_mutex);

        int* client_fd = malloc(sizeof(int));
        *client_fd = dequeue_connection(shm, sems);

        static thread_pool_t *pool = NULL;

        if (pool == NULL) {
            pool = create_thread_pool(config.threads_per_worker);
        }

        int *fd_copy = malloc(sizeof(int));
        *fd_copy = *client_fd;

        add_task(pool, (void*)send_file, fd_copy);

        //sem_post(sems->queue_mutex);
        //sem_post(sems->empty_slots);
    }
}*/