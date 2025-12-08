#ifndef UDS_H
#define UDS_H

#include "config.h"
#include "semaphores.h"

void send_fd(int socket, int fd_to_send, semaphores_t* sems, server_config_t config);
int recv_fd(int uds_sock);

#endif // FD_PASSING_H
