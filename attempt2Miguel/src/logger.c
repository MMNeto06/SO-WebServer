#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "semaphores.h"
#include "config.h"

int convertLogLevel(char* log_level){
    if(strcmp(log_level, "DEBUG") == 0){
        return 0;
    }
    else if(strcmp(log_level, "INFO") == 0){
        return 1;
    }
    else if(strcmp(log_level, "WARN") == 0){
        return 2;
    }
    else{
        return 3; //by default, logamos sempre erros
    }
}

void log_rotation(FILE* log_file, char* name){ //função interna!!!
    fseek(log_file, 0, SEEK_END);
    long log_size = ftell(log_file);
    fseek(log_file, 0, SEEK_SET); 
    if (log_size > 10 * 1024 * 1024) {  
        rename(name, strcat(name, ".1")); 
        log_file = fopen("access.log", "a"); 
    }
}

void log_request(sem_t* log_sem, const char* client_ip, const char* method, const char* path, int status, size_t bytes) {
    server_config_t config;
    load_config("../server.conf", &config);

    if(convertLogLevel(config.log_level) > 1){
        return;
    }

    time_t now = time(NULL); 
    struct tm* tm_info = localtime(&now); 
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%d/%b/%Y:%H:%M:%S %z", tm_info); 
     
    sem_wait(log_sem); 
    FILE* log = fopen(config.log_file, "a");
    log_rotation(log, config.log_file);
    if (log) { 
        fprintf(log, "%s - - [%s] \"%s %s HTTP/1.1\" %d %zu\n", client_ip, timestamp, method, path, status, bytes); 
        fclose(log); 
    } 
    sem_post(log_sem); 
}

void log_message(sem_t* log_sem, char* log_level, char* message){
    server_config_t config;
    load_config("../server.conf", &config);

    if(convertLogLevel(log_level) > convertLogLevel(config.log_level)){
        return;
    }

    time_t now = time(NULL); 
    struct tm* tm_info = localtime(&now); 
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%d/%b/%Y:%H:%M:%S %z", tm_info); 
     
    sem_wait(log_sem); 
    FILE* log = fopen(config.log_file, "a");
    log_rotation(log, config.log_file);
    if (log) { 
        fprintf(log, "[%s] [%s] - %s\n", log_level, timestamp, message); 
        fclose(log); 
    } 
    sem_post(log_sem); 

}