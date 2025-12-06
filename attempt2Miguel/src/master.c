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
#include <getopt.h>

#include "config.h"
#include "cache.h"
#include "shared_mem.h"
#include "semaphores.h"
#include "worker.h"
#include "stats.h"
#include "logger.h"
 
semaphores_t* sems; 
shared_data_t* shm;
worker_t* workers;
cache_t* cache;
 
void master_signal_handler(int signum) { 
    printf("Signum: %d\n", signum);
    shm->stats.keep_running = 0;
} 
 
int create_server_socket(int port, server_config_t config) { 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); 

    if (sockfd < 0){
        perror("socket");
        log_message(sems->log_mutex, "ERROR", "Socket error", config);
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
        log_message(sems->log_mutex, "ERROR", "Sockfd closed due to bind in create_server_socket", config);
        return -1; 
    } 
     
    if (listen(sockfd, 128) < 0) { 
        close(sockfd); 
        log_message(sems->log_mutex, "ERROR", "Sockfd closed due to listen in create_server_socket", config);
        return -1; 
    } 
 
    return sockfd; 
} 

void send_fd(int socket, int fd_to_send, server_config_t config) {
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
        log_message(sems->log_mutex, "ERROR", "send_fd sendmsg error", config);
        return;
    }
}

void create_workers(int num_workers, server_config_t config){
    int i;

    workers = malloc(sizeof(worker_t)*num_workers);

    for (i = 0; i < num_workers; i++) {
        workers[i].id = i;
        workers[i].shm = shm;
        workers[i].sems = sems;
        workers[i].cache = cache;

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
            log_message(sems->log_mutex, "ERROR", "Create workers bind error", config);
            perror("bind"); exit(1);
        }

        if (listen(sock, 10) < 0) {
            log_message(sems->log_mutex, "ERROR", "Creater workers listen error", config);
            perror("listen"); exit(1);
        }

        workers[i].uds_socket = sock; 
    }

    for (i = 0; i < num_workers; i++){
        switch (fork())
        {
        case -1:
            log_message(sems->log_mutex, "ERROR", "Fork error in create_workers", config);
            perror("fork");
            continue;

        case 0: 
            worker_main(workers[i], config);
            exit(0);

        default:
            break;
        }
    }

    for (i = 0; i < num_workers; i++) {
        int conn_fd = accept(workers[i].uds_socket, NULL, NULL);
        if (conn_fd < 0) { log_message(sems->log_mutex, "ERROR", "Create_workers accept error", config); perror("accept"); exit(1); }

        close(workers[i].uds_socket);
        workers[i].uds_socket = conn_fd;
    }
}

void master_cleanup(void){
    destroy_semaphores(sems);
    destroy_shared_memory(shm);
    cache_destroy(cache);
}

int master_init(server_config_t config){
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

    cache = cache_init(config.cache_size_mb * config.num_workers);
    if(cache == NULL){
        perror("cache_init");
        return -1;
    }

    int server_fd = create_server_socket(config.port, config); //está no master.c, inicializa a socket, pode não ficar aqui? idk
    
    if (server_fd < 0) {
        perror("create_server_socket");
        return -1;
    }
 
    printf("HTTP server running on http://localhost:%d\n", config.port); 
    printf("Server RealTime Stats available on http://localhost:%d/stats.html\n", config.port);
    log_message(sems->log_mutex, "INFO", "HTTP server running", config);
    log_message(sems->log_mutex, "INFO", "Server RealTime Stats available", config);

    create_workers(config.num_workers, config);

    return server_fd;
}

void master_accept_loop(int listen_fd, server_config_t config){
    /*server_config_t config;
    load_config("../server.conf", &config);*/
    int flags = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);
    int next_worker = 0; 
    time_t last_stats = time(NULL);
    while (shm->stats.keep_running) { 
        int client_fd = accept(listen_fd, NULL, NULL); 
        
        if (client_fd >= 0){
            printf("DEBUG: Accepted client_fd = %d\n", client_fd);
            log_message(sems->log_mutex, "DEBUG", "Accepted a client_fd", config);
            int worker_idx = next_worker % config.num_workers;
            send_fd(workers[worker_idx].uds_socket, client_fd, config);
            next_worker++;    
            close(client_fd);
        }else {
            if (errno != EAGAIN && errno != EWOULDBLOCK){
                log_message(sems->log_mutex, "ERROR", "master_accept_loop accept error", config);
                perror("accept");
            
            }
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

int main(int argc, char *argv[]){
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

    

    int opt;
    char opt_path[512] = "no";
    int opt_port = -1;
    int opt_workers = -1;
    int opt_threads = -1;
    int opt_verbose = -1;
    struct option long_options[] = {
            {   "config", required_argument, 0, 'c'},
            {     "port", required_argument, 0, 'p'},
            {  "workers", required_argument, 0, 'w'},
            {  "threads", required_argument, 0, 't'},
            {   "daemon", no_argument, 0, 'd'},
            {  "verbose", no_argument, 0, 'v'},
            {     "help", no_argument, 0, 'h'},
            {  "version", no_argument, 0, 'V'},
            {0,0,0,0}
    };
    

    while ((opt = getopt_long(argc, argv, "c:p:w:t:dvhV", long_options, NULL)) != -1){
        switch (opt){
            case 'c':
                strcpy(opt_path, optarg);
                break;
            case 'p':
                opt_port = atoi(optarg);
                break;
            case 'w':
                opt_workers = atoi(optarg);
                break;
            case 't':
                opt_threads = atoi(optarg);
                break;
            case 'd':
                //idk
                printf("Daemon is currently unavailable");
                break;
            case 'v': 
                opt_verbose = 0;
                break;
            case 'h':
                printf("Here is some help about server options\n-c, --config PATH : Configuration file path (default: ./server.conf)\n-p, --port PORT : Override port to listen on\n-w, --workers NUM : Override number of worker processes\n-t, --threads NUM : Override number of threads per worker\n-d, --daemon : Run in background\n-v, --verbose : Enable verbose logging\n-h, --help : Shows a help message with these options\n-V, --version : Show version information\n");
                break;
            case 'V':
                printf("ConcurrentHTTP Server System Version 33"); //MUDAR ISTO ANTES DE ENTREGAR
                break;
            default:
                break;

        }
    }

    server_config_t config;
    
    if(strcmp(opt_path, "no") != 0){
        load_config(opt_path, &config);
    }
    else{
        load_config("../server.conf", &config); //DÁ RUN DENTRO DE SRC SE NÃO QUERES ALTERAR ISTO
    }

    if(opt_port != -1){
        config.port = opt_port;
    }
    if(opt_threads != -1){
        config.threads_per_worker = opt_threads;
    }
    if(opt_workers != -1){
        config.num_workers = opt_workers;
    }
    if(opt_verbose == 0){
        strcpy(config.log_level, "DEBUG");
    }
    
    int server_fd = master_init(config);
    if (server_fd < 0){
        log_message(sems->log_mutex, "ERROR", "main reports master_init error", config);
        perror("master_init");
        exit(1);
    }
    master_accept_loop(server_fd, config);

    return 0; 
}