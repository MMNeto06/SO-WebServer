# Multi-Threaded Web Server with IPC and Semaphores 
 
**Sistems Operativos – TP2** 
 
A production-grade concurrent HTTP/1.1 web server implementing advanced 
process and thread synchronization using POSIX semaphores, shared memory, 
and thread pools. 
 
--- 
 
##         Table of Contents 
 
- [Overview](#overview) 
- [Features](#features) 
- [System Requirements](#system-requirements) 
- [Project Structure](#project-structure) 
- [Compilation](#compilation) 
- [Configuration](#configuration) 
- [Usage](#usage) 
- [Testing](#testing) 
- [Implementation Details](#implementation-details) 
- [Known Issues](#known-issues) 
- [Authors](#authors) 
- [Acknowledgments](#acknowledgments) 
 
--- 
 
##        Overview 
 
This project implements a multi-process, multi-threaded HTTP/1.1 web 
server that demonstrates: 
 
- **Process Management**: Master-worker architecture using `fork()` 
- **Inter-Process Communication**: Shared memory and POSIX semaphores 
- **Thread Synchronization**: Pthread mutexes, condition variables, reader-writer locks 
- **Concurrent Request Handling**: Thread pools with producer-consumer pattern 
- **HTTP Protocol**: Full HTTP/1.1 support including GET and HEAD methods 
- **Resource Management**: Thread-safe LRU file cache and statistics tracking 
 
### Architecture 
 
![DataFlow Diagram](./DataFlow%20Diagram.png)

--- 
##       Features 
 
### Core Features 
-      **Multi-Process Architecture**: 1 master + N workers (default: 4) 
-      **Thread Pools**: M threads per worker (default: 10) 
-      **HTTP/1.1 Support**: GET and HEAD methods 
-      **Status Codes**: 200, 404, 403, 500, 503 // VER ISTO MELHOR
-      **MIME Types**: HTML, CSS, JavaScript, images (PNG, JPG), PDF 
-      **Directory Index**: Automatic index.html serving 
-      **Custom Error Pages**: Branded 404 and 500 pages 
 
### Synchronization Features 
-      **POSIX Semaphores**: Inter-process synchronization 
-      **Pthread Mutexes**: Thread-level mutual exclusion 
-      **Condition Variables**: Producer-consumer queue signaling 
-      **Reader-Writer Locks**: Thread-safe file cache access 
 
### Advanced Features 
-      **Thread-Safe LRU Cache**: 10MB cache per worker with intelligent eviction 
-      **Apache Combined Log Format**: Industry-standard logging 
-      **Shared Statistics**: Real-time request tracking across all workers 
-      **Configuration File**: Flexible server.conf for easy customization 
-      **Log Rotation**: Automatic rotation at 10MB 
-      **Graceful Shutdown**: Proper cleanup on SIGINT/SIGTERM 
 
### Bonus Features
-     **HTTP Keep-Alive**: Persistent connections 
-     **HTTPS Support**: SSL/TLS encryption 
-     **Real-time Dashboard**: Web-based statistics viewer
-     **Range Requests**: Partial Content / HTTP 206 status request 
 
--- 
 
##       System Requirements 
 
### Required Software 
 
- **Operating System**: Linux (Ubuntu 20.04+ or similar) 
- **Compiler**: GCC 9.0+ with C11 support 
- **Libraries**:  
  - POSIX threads (`pthread`) 
  - POSIX real-time extensions (`rt`) 
  - Standard C library 
 
### Development Tools 
 
- **Build System**: GNU Make 
- **Testing**: Apache Bench (`ab`), curl, wget 
- **Debugging**: GDB, Valgrind, Helgrind 
- **Version Control**: Git (recommended) 
 
### Installation of Prerequisites 
 
```bash 
# Ubuntu/Debian 
sudo apt-get update 
sudo apt-get install -y build-essential gcc make 
sudo apt-get install -y apache2-utils curl valgrind 
 
# Verify installation 
gcc --version 
ab -V 
valgrind --version 
``` 
--- 
##       Project Structure 
``` 
concurrent-http-server/ 
│ 
├── src/                        # Source code 
│   ├── master.c/h              # Master process implementation & Program entry point 
│   ├── worker.c/h              # Worker process implementation
│   ├── http.c/h                # HTTP request/response handling
│   ├── thread_pool.c/h         # Thread pool management 
│   ├── cache.c/h               # LRU cache implementation 
│   ├── logger.c/h              # Thread-safe logging 
│   ├── sse.c/.h                # Server Sent Events handler
│   ├── semaphores.c/.h         # Semaphores implementation
│   ├── shared_mem.c/.h         # Shared Memory implementation
│   ├── stats.c/h               # Shared statistics 
│   └── config.c/h              # Configuration file parser 
│ 
├── www/                        # Document root 
│   ├── index.html              # Main page 
│   ├── index.css               # Stylesheets 
│   ├── index.js                # JavaScript files 
│   └── images/                 # Image/video assets 
│ 
├── tests/                      # Test suite 
│   ├── test_load.sh            # Load testing script 
│   ├── test_concurrent.c       # Concurrency tests 
│   └── README.md               # Test documentation 
│ 
├── docs/                       # Documentation //VER ISTO MELHOR - AINDA NÃO EXISTE
│   ├── design.pdf              # Architecture and design 
│   ├── report.pdf              # Technical report 
│   └── user_manual.pdf         # User guide 
│ 
├── Makefile                    # Build system 
├── server.conf                 # Configuration file 
├── README.md                   # This file 
├── cert.pem                    # Self signed certificate for TLS/SSL encryption
├── key.pem                     # Self signed key for TLS/SSL encryption
└── .gitignore                  # Git ignore rules 
``` 
 
--- 
 
 
 
##      Compilation 
 For more details check the User Manual
### Quick Start 
 
```bash 
# Build the server 
make 
 
# Clean build artifacts 
make clean 
 
# Rebuild from scratch 
make clean && make 
 
# Build and run 
make run 
``` 
 
### Build Targets 
 
```bash 
make all          # Build server executable (default) 
make clean        # Remove object files and executable 
make run          # Build and start server 
make test         # Build and run test suite 
make debug        # Build with debug symbols (-g) 
make release      # Build with optimizations (-O3) 
make valgrind     # Build and run under Valgrind 
make helgrind     # Build and run under Helgrind 
``` 
 
### Manual Compilation 
 
```bash 
# Compile all source files 
gcc -Wall -Wextra -pthread -lrt -o server \ 
    src/main.c \ 
    src/master.c \ 
    src/worker.c \ 
    src/http.c \ 
    src/thread_pool.c \ 
    src/cache.c \ 
    src/logger.c \ 
    src/stats.c \ 
    src/config.c 
 
# Run the server 
./server 
``` 
 
### Compiler Flags Explained 
- `-Wall -Wextra`: Enable all warnings 
- `-pthread`: Link pthread library 
- `-lrt`: Link POSIX real-time extensions (for semaphores) 
- `-O3`: Optimization level 3 (for release builds) 
- `-g`: Debug symbols (for debugging) 
- `-fsanitize=thread`: Thread sanitizer (for race detection) 
--- 
##        Configuration 
 For more details check the User Manual
### Configuration File (server.conf) 
 
The server reads configuration from `server.conf` in the current 
directory: 
 
```ini 
# Server Configuration File 
 
# Network settings 
PORT=8080                          # Port to listen on 
TIMEOUT_SECONDS=30                 # Connection timeout 
 
# File system 
DOCUMENT_ROOT=/var/www/html        # Root directory for serving files 
 
# Process architecture 
NUM_WORKERS=4                      # Number of worker processes 
THREADS_PER_WORKER=10              # Threads per worker 
 
# Queue management 
MAX_QUEUE_SIZE=100                 # Connection queue size 
 
# Caching 
CACHE_SIZE_MB=10                   # Cache size per worker (MB) 
 
# Logging 
LOG_FILE=access.log                # Access log file path 
LOG_LEVEL=INFO                     # Log level: DEBUG, INFO, WARN, ERROR 
``` 
 
### Configuration Parameters 
 
| Parameter          | Default    | Description                      | 
|--------------------|------------|----------------------------------| 
| PORT               | 8080       | TCP port for HTTP server         | 
| DOCUMENT_ROOT      | ./www      | Root directory for serving files | 
| NUM_WORKERS        | 4          | Number of worker processes       | 
| THREADS_PER_WORKER | 10         | Thread pool size per worker      | 
| MAX_QUEUE_SIZE     | 100        | Connection queue capacity        | 
| CACHE_SIZE_MB      | 10         | Maximum cache size per worker    | 
| LOG_FILE           | access.log | Path to access log               | 
| TIMEOUT_SECONDS    | 30         | Connection timeout               | 
 
### Environment Variables 
 
```bash 
# Override configuration via environment 
export HTTP_PORT=8080 
export HTTP_WORKERS=4 
export HTTP_THREADS=10 
 
./server 
``` 
 
---

##            Usage 
 
### Starting the Server 
 
```bash 
# Start with default configuration 
./server 
 
# Start with custom configuration file 
./server -c /path/to/server.conf 
 
# Start on specific port 
./server -p 9090 
 
# Start with verbose logging 
./server -v 
``` 
 
### Command-Line Options 
 
``` 
Usage: ./server [OPTIONS] 
 
Options: 
  -c, --config PATH      Configuration file path (default: ./server.conf) 
  -p, --port PORT       Port to listen on (default: 8080) 
  -w, --workers NUM     Number of worker processes (default: 4) 
  -t, --threads NUM     Threads per worker (default: 10) 
  -v, --verbose         Enable verbose logging 
  -h, --help            Show this help message 
  --version             Show version information 
``` 
 
### Accessing the Server 
 
```bash 
# Open in browser 
firefox https://localhost:8080 
 
# Using curl 
curl https://localhost:8080/index.html 
 
# View headers only (HEAD request) 
curl -I https://localhost:8080/index.html 
 
# Download file 
wget https://localhost:8080/document.pdf 
``` 
 
### Stopping the Server 
 
```bash 
# Graceful shutdown (from server terminal) 
Ctrl+C 
# Send SIGTERM 
kill $(pgrep -f "./server") 
 
# Force kill (not recommended) 
kill -9 $(pgrep -f "./server") 
``` 
 
### Viewing Logs 
 
```bash 
# Follow access log in real-time 
tail -f access.log 
 
# View last 100 entries 
tail -n 100 access.log 
 
# Search for errors 
grep "500\|404" access.log 
 
# Count requests by status code 
awk '{print $9}' access.log | sort | uniq -c 
``` 
 
### Monitoring Statistics 
 
Statistics are displayed every 30 seconds on stdout: 
 
``` 
======================================== 
SERVER STATISTICS 
======================================== 
Uptime: 120 seconds 
Total Requests: 1,542 
Successful (2xx): 1,425 
Client Errors (4xx): 112 
Server Errors (5xx): 5 
Bytes Transferred: 15,728,640 
Average Response Time: 8.3 ms 
Active Connections: 12 
Cache Hit Rate: 82.4% 
======================================== 
``` 
 
--- 
 
##       Testing 
 If you want to do the following tests automatically, please check the scripts in the tests directory
### Functional Tests 
 
```bash 
# Run all tests 
make test 
 
# Basic functionality test 
curl https://localhost:8080/index.html 
 
# Test HEAD method 
curl -I https://localhost:8080/index.html 
# Test 404 error 
curl https://localhost:8080/nonexistent.html 
 
# Test different file types 
curl https://localhost:8080/style.css        # CSS 
curl https://localhost:8080/script.js        # JavaScript 
curl https://localhost:8080/image.png        # Image 
``` 
 
### Load Testing 
 
```bash 
# Basic load test (1000 requests, 10 concurrent) 
ab -n 1000 -c 10 https://localhost:8080/index.html 
 
# High concurrency test (10000 requests, 100 concurrent) 
ab -n 10000 -c 100 https://localhost:8080/index.html 
 
# Sustained load test (5 minutes) 
ab -t 300 -c 50 https://localhost:8080/ 
 
# Test multiple files 
for file in index.html style.css script.js; do 
    ab -n 1000 -c 50 https://localhost:8080/$file 
done 
``` 
 
### Concurrency Testing 
 
```bash 
# Parallel requests with curl 
for i in {1..100}; do 
    curl -s https://localhost:8080/index.html & 
done 
wait 
 
# Parallel requests with different files 
for i in {1..50}; do 
    curl -s https://localhost:8080/page$((i % 10)).html & 
done 
wait 
``` 
 
### Memory Leak Detection 
 
```bash 
# Run server under Valgrind 
make valgrind 
 
# Or manually: 
valgrind --leak-check=full \ 
         --show-leak-kinds=all \ 
         --track-origins=yes \ 
         --log-file=valgrind.log \ 
         ./server 
 
# In another terminal, generate traffic 
ab -n 1000 -c 50 https://localhost:8080/ 
 
# Stop server and check valgrind.log 
``` 
 
### Race Condition Detection 
 
```bash 
# Run server under Helgrind 
make helgrind 
 
# Or manually: 
valgrind --tool=helgrind \ 
         --log-file=helgrind.log \ 
         ./server 
 
# Generate concurrent traffic 
ab -n 5000 -c 100 https://localhost:8080/ 
 
# Check helgrind.log for data races 
``` 
 
### Performance Testing 
 
```bash 
# Measure cache effectiveness 
# First request (cache miss) 
time curl -s https://localhost:8080/index.html > /dev/null 
 
# Subsequent requests (cache hit) 
for i in {1..10}; do 
    time curl -s https://localhost:8080/index.html > /dev/null 
done 
 
# Monitor server resource usage 
top -p $(pgrep -f "./server") 
 
# Monitor worker processes 
watch -n 1 'pgrep -P $(pgrep -f "./server") | xargs ps -o 
pid,ppid,nlwp,cmd' 
``` 
 
--- 
 
##            Implementation Details 
 
### Master Process 
 
**Responsibilities:** 
- Initialize listening socket on configured port 
- Create shared memory segments for connection queue and statistics 
- Initialize POSIX semaphores for synchronization 
- Fork N worker processes 
- Accept incoming TCP connections 
- Enqueue connections in shared queue 
- Handle signals (SIGINT, SIGTERM) for graceful shutdown 
- Display statistics periodically
 
 
**Key Functions:** 
 
```c 
int main(int argc, char *argv[]);
int master_init(server_config_t *config); 
void master_accept_loop(int listen_fd); 
void master_signal_handler(int signum); 
void master_cleanup(void); 
int create_server_socket(int port, server_config_t config);
void send_fd(int socket, int fd_to_send, server_config_t config);
void create_workers(int num_workers, server_config_t config);
``` 
 
### Worker Processes 
 
**Responsibilities:** 
- Create thread pool with M threads 
- Initialize thread-safe LRU cache 
- Threads dequeue connections from shared queue 
- Parse HTTPS requests
- Serve files from document root 
- Update shared statistics 
- Handle graceful shutdown 
 
**Key Functions:** 
```c 
void* worker_thread(void *arg);
void worker_process_request(void* arg);
void worker_main(worker_t worker, server_config_t config);
``` 
 
### Thread Pool 
 
**Implementation:** 
- Fixed-size thread pool (no dynamic creation) 
- Condition variables for empty/full queue 
- Mutex protection for queue access 
- Threads block when no work available 
 
**Queue Operations:** 
```c 
thread_pool_t* create_thread_pool(int num_threads);
void add_task(thread_pool_t* pool, void (*function)(void*), void* arg);
int thread_pool_dequeue(thread_pool_t* pool);
void destroy_thread_pool(thread_pool_t* pool);
``` 
 
### HTTP Request Handling 
 
**Supported Methods:** 
- **GET**: Retrieve resource, return headers + body 
- **HEAD**: Retrieve headers only, no body 
 
**Request Parsing:** 
```c 
http_request_t* http_parse_request(char *buffer, server_config_t config);
char* http_build_response(http_request_t *req, int *status_code, long *out_size, server_config_t config);
void http_send_response(SSL *ssl, char *response, long size);

``` 
 
 
 
**MIME Type Detection:** 
```c 
const char* get_mime_type(const char *filename); 
// .html → text/html 
// .css  → text/css 
// .js   → application/javascript 
// .png  → image/png 
// .jpg  → image/jpeg 
// .pdf  → application/pdf 
``` 
 
### LRU Cache Implementation 
 
**Algorithm:** 
- Doubly-linked list for LRU ordering 
- Hash table for O(1) lookup 
- Reader-writer lock for thread safety 
- Maximum size enforced (10MB per worker) 
 
**Cache Operations:** 
```c 
cache_t* cache_init(size_t max_size_mb); 
cache_entry_t* cache_get(cache_t *cache, const char *key); 
void cache_put(cache_t *cache, const char *key, void *data, size_t size); 
void cache_evict_lru(cache_t *cache); 
void cache_destroy(cache_t *cache); 
``` 
 
**Synchronization:** 
```c 
pthread_rwlock_rdlock(&cache->lock);  // Multiple readers 
// Read cached file 
pthread_rwlock_unlock(&cache->lock); 
 
pthread_rwlock_wrlock(&cache->lock);  // Exclusive writer 
// Update cache 
pthread_rwlock_unlock(&cache->lock); 
``` 
 
### Logging 
 
**Format: Apache Combined Log** 
``` 
127.0.0.1 - - [22/Nov/2025:14:30:45 -0800] "GET /index.html HTTP/1.1" 200 2048 
``` 

**For other messages that aren't requests, the format is:**
```
[INFO] [07/Dec/2025:15:39:28 +0000] - Worker 0 initiated
```
There are 4 log levels: DEBUG, INFO, WARN and ERROR
 
**Thread-Safe Implementation:** 
```c 
sem_wait(&log_semaphore);      // Acquire lock 
fprintf(log_file, ...);        // Write log entry 
fflush(log_file);              // Ensure written 
sem_post(&log_semaphore);      // Release lock 
``` 

**Log Rotation:** 
```c 
    FILE* f = fopen(filename, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    if (size < 10 * 1024 * 1024) {
        return; //<10MB
    }
    char rotated[1024];
    snprintf(rotated, sizeof(rotated), "%s.1", filename);
    remove(rotated);
    rename(filename, rotated);
``` 
 
### Shared Statistics 
 
**Tracked Metrics:** 
```c 
typedef struct { 
    long total_requests; 
    long bytes_transferred; 
    long status_200; 
    long status_404; 
    long status_500; 
    long status_206;
    long total_response_time;
    int active_connections; 
    int keep_running;
} server_stats_t; 
``` 
 
**Atomic Updates:** 
```c 
sem_wait(&stats_semaphore); 
stats->total_requests++; 
stats->bytes_transferred += bytes; 
stats->status_counts[status_code]++; 
sem_post(&stats_semaphore); 
``` 
 
--- 
 
##          Known Issues 
 
### Current Limitations 
 
//VER ISTO MELHOR
isto tem que ser atualizado no fim, não sei o que meter
 
### Platform-Specific Issues 
 
- **macOS**: Limited support due to differences in POSIX semaphore 
implementation 
- **Windows**: Not supported (requires POSIX APIs) 
 
### Performance Considerations 
 
- **Large File Handling**: Files > 1MB not cached, may be slow 
- **Many Small Files**: Cache thrashing possible with > 1000 distinct files 
- **Queue Overflow**: 503 errors possible under extreme load (> 500 concurrent connections) 
 
--- 
##         Authors 
 
**Miguel Neto** 
- Student NMecID: 125200 
- Email: [miguelsneto@ua.pt]
- GitHub: [MMNeto06](https://github.com/MMNeto06) 
 
**Diogo Silva**
- Student NMecID: 125240 
- Email: [diogo.c.silva@ua.pt] 
- GitHub: [dsilva08](https://github.com/dsilva08) 
 
--- 
 
##       Acknowledgments 
 
### References 
 
- [POSIX Threads Programming](https://computing.llnl.gov/tutorials/pthreads/) 
- Official bibliographic references os the course 
- [HTTP/1.1 RFC 2616](https://www.rfc-editor.org/rfc/rfc2616) 
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) 
- The Linux Programming Interface (Michael Kerrisk) 
- Operating Systems: Three Easy Pieces (Arpaci-Dusseau) 
- Other documentation used... 
 
### Tools Used 
 
- **Compiler**: GCC 11.4.0 
- **Debugger**: GDB 12.1, Valgrind 3.19 
- **Build System**: GNU Make 4.3 
- **Editor**: [VS Code / Vim / etc.] 
- **Version Control**: Git 2.34 
 
### Course Information 
 
- **Course**: Sistemas Operativos (40381-SO ) 
- **Semester**: 1/2025 
- **Instructor**: Prof. Pedro Fernandes and Prof. Nuno Lau 
- **Institution**: Universidade de Aveiro 
 
--- 
 
##       License 
 
This project is submitted as coursework for Sistemas Operativos at 
Universidade de Aveiro. 
 
All code is original work by the authors unless otherwise noted in source 
file comments. 
 
--- 