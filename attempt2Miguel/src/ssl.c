#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <unistd.h> 
#include "worker.h"
#include "logger.h"

SSL_CTX* ssl_init(char* cert, char* key) {
    SSL_CTX *ssl_ctx;
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    ssl_ctx = SSL_CTX_new(TLS_server_method());

    SSL_CTX_use_certificate_file(ssl_ctx, cert, SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ssl_ctx, key, SSL_FILETYPE_PEM);

    return ssl_ctx;
}

void log_ssl_errors(worker_task_args_t* args){
    unsigned long ssl_err_code;
    char ssl_err_buf[256];

    while ((ssl_err_code = ERR_get_error()) != 0) {
        ERR_error_string_n(ssl_err_code, ssl_err_buf, sizeof(ssl_err_buf));
        log_message(args->sems->log_mutex, "ERROR", ssl_err_buf, args->config);
    }
}

SSL* ssl_setup(worker_task_args_t* args){
    struct timeval tv = {args->config.timeout_seconds, 0 };
    setsockopt(args->client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    SSL *ssl = SSL_new(args->ssl_ctx);
    if (!ssl) {
        log_message(args->sems->log_mutex, "ERROR", "SSL_new() failed", args->config);
        close(args->client_fd);
        free(args);
        return NULL;
    }

    SSL_set_fd(ssl, args->client_fd);
    if (SSL_accept(ssl) <= 0) {
        int err = SSL_get_error(ssl, -1);
        if (err != SSL_ERROR_ZERO_RETURN && err != SSL_ERROR_SYSCALL) {
            log_ssl_errors(args);
        }

        SSL_free(ssl);
        close(args->client_fd);
        free(args);
        return NULL;
    }

    return ssl;
}

int handle_ssl_read_errors(int bytes, SSL* ssl, worker_task_args_t* args){
    if (bytes > 0) return 0;
    char logmsg[4096];

    int ssl_err = SSL_get_error(ssl, bytes);
    if (ssl_err == SSL_ERROR_ZERO_RETURN || (ssl_err == SSL_ERROR_SYSCALL && bytes == 0)){
        return 1; // connection closed
    }
    if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE){
        return 0; // retry
    }

    log_ssl_errors(args);
    sprintf(logmsg, "SSL_read() error on fd %d (ssl_err=%d)", args->client_fd, ssl_err);
    log_message(args->sems->log_mutex, "ERROR", logmsg, args->config);
    return 1; // terminate on real error
}

void ssl_teardown(worker_task_args_t* args, SSL* ssl){
    char logmsg[4096];
    int s = SSL_shutdown(ssl);
    if (s == 0) s = SSL_shutdown(ssl);
    if (s < 0){
        int err = SSL_get_error(ssl, s);
        if (!(err == SSL_ERROR_SYSCALL || err == SSL_ERROR_ZERO_RETURN)){
            log_ssl_errors(args);
            sprintf(logmsg, "SSL_shutdown() error on fd %d (err=%d)", args->client_fd, err);
            log_message(args->sems->log_mutex, "ERROR", logmsg, args->config);
        }
    }
    SSL_free(ssl);
    close(args->client_fd);
}