#include "shared_mem.h" 
#include <sys/mman.h> 
#include <fcntl.h> 
#include <unistd.h> 
#include <string.h> 
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
 
#define SHM_NAME "/webserver_shm" 
 
shared_data_t* create_shared_memory() { 
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666); 
  
    if (shm_fd == -1) return NULL; 
     
    if (ftruncate(shm_fd, sizeof(shared_data_t)) == -1) { 
        close(shm_fd); 
        return NULL; 
    } 
     
    shared_data_t* data = mmap(NULL, sizeof(shared_data_t), 
        PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); 
    close(shm_fd); 
     
    if (data == MAP_FAILED) return NULL; 
     
    memset(data, 0, sizeof(shared_data_t)); 
    return data; 
}

shared_data_t* open_shared_memory(){
    shared_data_t* shm; 

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1){
        perror("shm_open");
        return NULL;
    }
     
    shm = mmap(NULL, sizeof(shared_data_t), 
        PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); 
    close(shm_fd); 
     
    if (shm == MAP_FAILED){
        perror("mmap");
        return NULL; 
    }

    return shm;
}
 
void destroy_shared_memory(shared_data_t* data) { 
    munmap(data, sizeof(shared_data_t)); 
    shm_unlink(SHM_NAME); 
}