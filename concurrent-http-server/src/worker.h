#ifndef WORKER_H
#define WORKER_H

#define SHM_NAME "/webserver_shm"

void send_file();
shared_data_t* open_shared_memory();
semaphores_t* open_semaphores();
void worker_loop(int worker_id);

#endif