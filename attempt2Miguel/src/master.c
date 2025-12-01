#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <signal.h> 
#include <pthread.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"
#include "shared_mem.h"
#include "semaphores.h"
#include "worker.h"
#include "stats.h"
 
semaphores_t* sems; 
shared_data_t* shm;
worker_t* workers;
 
void master_signal_handler(int signum) { 
    printf("Signum: %d\n", signum);
    shm->stats.keep_running = 0;
} 
 
int create_server_socket(int port) { 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); 

    if (sockfd < 0){
        perror("socket");
        return -1; 
    }

    int opt = 1; 
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 
     
    struct sockaddr_in addr; 
    addr.sin_family = AF_INET; 
    addr.sin_addr.s_addr = INADDR_ANY; 
    addr.sin_port = htons(port); 
     
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { 
        close(sockfd); 
        return -1; 
    } 
     
    if (listen(sockfd, 128) < 0) { 
        close(sockfd); 
        return -1; 
    } 
 
    return sockfd; 
} 

void send_fd(int socket, int fd_to_send) {
    struct msghdr msg = {0};
    struct iovec iov;
    char buf[1] = {0};
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char cbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof(cbuf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));

    if (sendmsg(socket, &msg, 0) < 0) {
        perror("sendmsg");
        return;
    }
}

void create_workers(int num_workers){
    int i;

    workers = malloc(sizeof(worker_t)*num_workers);

    for (i = 0; i < num_workers; i++) {
        workers[i].id = i;
        workers[i].shm = shm;
        workers[i].sems = sems;

        char path[108];
        snprintf(path, sizeof(path), "/tmp/worker_socket_%d", i);

        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) { perror("socket"); exit(1); }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

        unlink(path); // garante que não existe socket antigo
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind"); exit(1);
        }

        if (listen(sock, 10) < 0) {
            perror("listen"); exit(1);
        }

        workers[i].uds_socket = sock; 
    }

    for (i = 0; i < num_workers; i++){
        switch (fork())
        {
        case -1:
            perror("fork");
            continue;

        case 0: 
            worker_main(workers[i]);
            exit(0);

        default:
            break;
        }
    }

    for (i = 0; i < num_workers; i++) {
        int conn_fd = accept(workers[i].uds_socket, NULL, NULL);
        if (conn_fd < 0) { perror("accept"); exit(1); }

        close(workers[i].uds_socket);
        workers[i].uds_socket = conn_fd;
    }
}

void master_cleanup(void){
    destroy_semaphores(sems);
    destroy_shared_memory(shm);
}

int master_init(server_config_t config){
    int server_fd = create_server_socket(config.port); //está no master.c, inicializa a socket, pode não ficar aqui? idk
    
    if (server_fd < 0) {
        perror("create_server_socket");
        return -1;
    }
 
    printf("HTTP server running on http://localhost:%d\n", config.port); 

    shm = create_shared_memory();
    if (shm == NULL){
        perror("create_shared_memory");
        return -1;
    }
    shm->stats.keep_running = 1;

    sems = malloc(sizeof(semaphores_t));
    if (init_semaphores(sems, config.max_queue_size) != 0){
        perror("init_semaphores");
        return -1;
    }

    create_workers(config.num_workers);

    return server_fd;
}

void master_accept_loop(int listen_fd){
    server_config_t config;
    load_config("../server.conf", &config);
    int flags = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);
    int next_worker = 0; 
    time_t last_stats = time(NULL);
    while (shm->stats.keep_running) { 
        int client_fd = accept(listen_fd, NULL, NULL); 
        
        if (client_fd >= 0){
            printf("DEBUG: Accepted client_fd = %d\n", client_fd);
            int worker_idx = next_worker % config.num_workers;
            send_fd(workers[worker_idx].uds_socket, client_fd);
            next_worker++;    
            close(client_fd);
        }else {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                perror("accept");
        }

        time_t now = time(NULL);
        if (now - last_stats >= 30){
            print_stats(shm,sems);
            last_stats = now;
        }
    }  
    master_cleanup();
    close(listen_fd);
}

int main(void){
    struct sigaction sigact;

    /* altera a rotina de atendimento ao ^C */
    sigact.sa_handler = master_signal_handler;
    sigemptyset (&sigact.sa_mask);
    sigact.sa_flags = 0;
    if (sigaction(SIGINT, &sigact, NULL) < 0) { 
        perror("Rotina de atendimento não instalada\n");
        return EXIT_FAILURE;
    }

    /* altera a rotina de atendimento ao ^Z */
    sigact.sa_handler = master_signal_handler;
    sigemptyset (&sigact.sa_mask);
    sigact.sa_flags = 0;
    if (sigaction(SIGTERM, &sigact, NULL) < 0) { 
        perror("Rotina de atendimento não instalada\n");
        return EXIT_FAILURE;
    }

    server_config_t config;
    load_config("../server.conf", &config); //DÁ RUN DENTRO DE SRC SE NÃO QUERES ALTERAR ISTO
    int server_fd = master_init(config);
    if (server_fd < 0){
        perror("master_init");
        exit(1);
    }
    master_accept_loop(server_fd);

    return 0; 
}