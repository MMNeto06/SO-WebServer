#include "sse.h"
#include "stats.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static void send_snapshot_to_client(SSL *ssl, shared_data_t *shm) {
    char msg[512];
    int len = snprintf(msg, sizeof(msg),
        "data: {\"total_requests\": %ld, \"total_bytes\": %ld, \"status_200\": %ld, \"status_206\": %ld, \"status_404\": %ld, \"status_500\": %ld, \"active_connections\": %d}\n\n",
        shm->stats.total_requests,
        shm->stats.bytes_transferred,
        shm->stats.status_200,
        shm->stats.status_206,
        shm->stats.status_404,
        shm->stats.status_500,
        shm->stats.active_connections
    );

    if (len > 0) {
        SSL_write(ssl, msg, len);
    }
}

void handle_sse(SSL *ssl, shared_data_t *shm, semaphores_t *sems) {
    update_active_connection(shm, sems);

    const char *headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";

    if (SSL_write(ssl, headers, strlen(headers)) <= 0) {
        SSL_shutdown(ssl); 
        return;
    }

    sem_wait(sems->stats_mutex);
    send_snapshot_to_client(ssl, shm); // evita que o cliente espere pelo processamento das stats
    sem_post(sems->stats_mutex);

    while (shm->stats.keep_running) {
        sem_wait(sems->stats_mutex);

        long total_requests = shm->stats.total_requests;
        long total_bytes = shm->stats.bytes_transferred;
        long status_200 = shm->stats.status_200;
        long status_206 = shm->stats.status_206;
        long status_403 = shm->stats.status_403;
        long status_404 = shm->stats.status_404;
        long status_500 = shm->stats.status_500;
        long status_503 = shm->stats.status_503;
        long active_connections = shm->stats.active_connections;
        double avg_response_time = (shm->stats.total_requests > 0) ? shm->stats.total_response_time / shm->stats.total_requests : 0;

        sem_post(sems->stats_mutex);

        char msg[512];
        int len = snprintf(msg, sizeof(msg),
            "data: {\"total_requests\": %ld, \"total_bytes\": %ld, \"status_200\": %ld, \"status_206\": %ld, \"status_403\": %ld, \"status_404\": %ld, \"status_500\": %ld, \"status_503\": %ld, \"active_connections\": %ld, \"average_response_time\": %f}\n\n",
            total_requests,
            total_bytes,
            status_200,
            status_206,
            status_403,
            status_404,
            status_500,
            status_503,
            active_connections,
            avg_response_time
        );

        if (len <= 0) break;

        int sent = SSL_write(ssl, msg, len);
        if (sent <= 0) break;

        sleep(1);
    }

    SSL_shutdown(ssl); 
    update_connection_terminated(shm, sems);
}
