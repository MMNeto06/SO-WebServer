#ifndef CONFIG_H 
#define CONFIG_H 
 
typedef struct { 
    int port; 
    char document_root[512]; 
    int num_workers; 
    int threads_per_worker; 
    int max_queue_size; 
    char log_file[512]; 
    int cache_size_mb; 
    int timeout_seconds; 
    char log_level[6];
} server_config_t; 
 
int load_config(const char* filename, server_config_t* config); 
 
#endif