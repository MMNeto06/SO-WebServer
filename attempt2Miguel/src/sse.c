#include "sse.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

static void send_snapshot_to_client(int client_fd, shared_data_t *shm) {
    char msg[512];
    int len = snprintf(msg, sizeof(msg),
        "data: {\"total_requests\": %ld, \"total_bytes\": %ld, \"status_200\": %ld, \"status_404\": %ld, \"status_500\": %ld, \"active_connections\": %d}\n\n",
        shm->stats.total_requests,
        shm->stats.bytes_transferred,
        shm->stats.status_200,
        shm->stats.status_404,
        shm->stats.status_500,
        shm->stats.active_connections
    );

    if (len > 0) {
        send(client_fd, msg, (size_t)len, 0);
    }
}

void handle_sse(int client_fd, shared_data_t *shm, semaphores_t *sems) {
    const char *headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";

    if (send(client_fd, headers, strlen(headers), 0) <= 0) {
        close(client_fd);
        return;
    }

    sem_wait(sems->stats_mutex);
    send_snapshot_to_client(client_fd, shm);
    sem_post(sems->stats_mutex);

    // Loop: enviar eventos enquanto o servidor estiver a correr
    while (shm->stats.keep_running) {
        sem_wait(sems->stats_mutex);

        // --- Ler snapshot das estatísticas (sem lock aqui; se tiveres mutex/sem, usa) ---
        long total_requests    = shm->stats.total_requests;
        long total_bytes       = shm->stats.bytes_transferred;
        long status_200        = shm->stats.status_200;
        long status_404        = shm->stats.status_404;
        long status_500        = shm->stats.status_500;
        long active_connections = shm->stats.active_connections;

        sem_post(sems->stats_mutex);

        char msg[512];
        int len = snprintf(msg, sizeof(msg),
            "data: {\"total_requests\": %ld, \"total_bytes\": %ld, \"status_200\": %ld, \"status_404\": %ld, \"status_500\": %ld, \"active_connections\": %ld}\n\n",
            total_requests,
            total_bytes,
            status_200,
            status_404,
            status_500,
            active_connections
        );

        if (len <= 0) break;

        ssize_t sent = send(client_fd, msg, (size_t)len, 0);
        if (sent <= 0) break;

        // flush best-effort: send is enough
        // intervalo entre eventos (ajusta se quiseres mais/menos frequência)

        sleep(1);
    }

    close(client_fd);
}
