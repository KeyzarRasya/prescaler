#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stddef.h>
#include "../include/http.h"
#include "../include/engine.h"
#include "../include/epoll.h"
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include "timer.h"
#include "metrics.h"
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#define DEBUG 0

/* Global round-robin counter for load balancing */
static atomic_uint_fast32_t rr_counter = ATOMIC_VAR_INIT(0);

/* Available backend ports */
static const int BACKEND_PORTS[] = {3000, 3001, 3002};
static const int NUM_BACKENDS = 3;

/* Per-port RPS counters - indexed by (port - 3000) */
static atomic_int rps_counters[3] = {
    ATOMIC_VAR_INIT(0),
    ATOMIC_VAR_INIT(0),
    ATOMIC_VAR_INIT(0)
};

double cpu_t0;

void *request_per_second(void *arg) {
    struct thread_arg *targ = (struct thread_arg *)arg;
    int port = targ->port;
    struct timeseries_db *tsdb = init_tsdb(8086);
    if (tsdb_connect(tsdb) != CONN_SUCCESS) {
        perror("tsdb start");
        return NULL;
    }
    tsdb->table = "server";

    char filename[32];
    snprintf(filename, sizeof(filename), "data_%d.txt", port);
    FILE *fptr = fopen(filename, "a");
    if (!fptr) {
        perror("file pointer");
        return NULL;
    }

    char metrics_res[MAX_METRICS_RESPONSE];
    if (request_metrics(port, metrics_res, sizeof(metrics_res)) != 0) {
        perror("CPU t0 initial");
        fclose(fptr);
        return NULL;
    }

    double cpu_t0 = get_value(metrics_res, CPU_SECOND);

    while (1) {
        sleep_ms(1000);

        struct metrics *metrics = init_metrics();
        if (request_metrics(port, metrics_res, sizeof(metrics_res)) != 0) {
            perror("metrics request");
            free(metrics);
            break;
        }
        metrics->port = port;
        metrics->timestamp = time(NULL);

        store_metrics(metrics, metrics_res);

        double current_cpu = metrics->cpu_usage;
        calculate_metrics(metrics, cpu_t0);

        /* Get RPS for THIS specific port */
        int port_index = port - 3000;
        int rps = atomic_exchange(&rps_counters[port_index], 0);
        metrics->rps = rps;
        write_metrics(fptr, metrics, rps);

        #if TSDB_ENABLE
            write_metrics_db(tsdb, metrics);
        #endif

        cpu_t0 = current_cpu;
        rps = 0;

        free(metrics);
        memset(metrics_res, 0, sizeof(metrics_res));
    }

    close(tsdb->fd);
    free(tsdb);
    fclose(fptr);
    return NULL;
}

/* Static Function */
static int handle_request(int fd, struct http_req *hreq) {
    char request[MAX_BUFF_SIZE];
    int n = recv(fd, request, sizeof(request), 0);
    if (n <= 0) {
        close(fd);
        return -1;
    }
    request[n] = '\0';
    convert_request(hreq, request, sizeof(request));
    return 0;
}

static void handle_response(int fd, struct http_req *hreq, int backend_port) {
    char response[MAX_BUFF_SIZE];
    if (forwards_to_port(hreq->raw, response, sizeof(response), backend_port) != 0) {
        snprintf(response, sizeof(response), "%s", ON_SOCK_ERR);
    }
    send(fd, response, sizeof(response), 0);
}

void log_elapsed_time(struct timespec start, struct timespec end) {
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Elapsed time: %0.6f\n", elapsed);
}

/**
 * Round-robin load balancing - O(1) time complexity
 * Thread-safe using atomic operations
 * Returns the next backend port in rotation
 */
static int get_next_backend_port(void) {
    uint32_t current = atomic_fetch_add(&rr_counter, 1);
    int index = current % NUM_BACKENDS;
    return BACKEND_PORTS[index];
}

/**
 * Connect to specific backend server port
 * This version takes the port as a parameter for better control
 */
static int connect_to_port(int fd, int port) {
    struct sockaddr_in destiny = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    inet_pton(AF_INET, "127.0.0.1", &destiny.sin_addr);

    if (connect(fd, (struct sockaddr*)&destiny, sizeof(destiny)) < 0) {
        return -1;
    }
    return 0;
}

void init_listener(int fd, struct prescal_engine *engine) {
    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port = htons(engine->port),
        .sin_addr = INADDR_ANY
    };
    
    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("socket option");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (bind(fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("bind");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (listen(fd, SOMAXCONN) < 0) {
        perror("listen");
        close(fd);
        exit(EXIT_FAILURE);
    }
}

void handle_connections(int fd,
        int epfd,
        struct epoll_event *ev,
        struct epoll_event *events
    ) {
    struct sockaddr_in client;
    printf("Listening...\n");

    while (1) {
        int npfd = epoll_wait(epfd, events, EPOLL_MAX_EVENTS, -1);
        for (int i = 0; i < npfd; i++) {
            if (events[i].data.fd == fd) {
                /* Accept all pending connections in this iteration */
                while (1) {
                    socklen_t client_size = sizeof(client);
                    int new_fd = accept(fd, (struct sockaddr*)&client, &client_size);
                    if (new_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            /* All connections processed */
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    /* Set non-blocking mode */
                    int flags = fcntl(new_fd, F_GETFL, 0);
                    fcntl(new_fd, F_SETFL, flags | O_NONBLOCK);

                    /* Add to epoll for monitoring */
                    ev->data.fd = new_fd;
                    ev->events = EPOLLIN | EPOLLET;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, new_fd, ev);
                }
            } else {
                int client_fd = events[i].data.fd; // Get the client file descriptor
                process_request(client_fd);
                // Now, remove from epoll and close the socket
                epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
                close(client_fd);
            }
        }
    }
}
/* END */


struct prescal_engine *engine_init(char *host, uint16_t port) {
    struct prescal_engine *engine = 
        malloc(sizeof(struct prescal_engine));

    if (!engine) {
        perror("Failed to allocate engine");
        return NULL;
    }
    engine->host = host;
    engine->port = port;
    engine->config = config_init();

    read_config(engine->config);
    return engine;
}

void start(struct prescal_engine *engine) {
    pthread_t metric_threads[3];
    struct thread_arg args[3] = {
        {3000}, {3001}, {3002}
    };

    for (int i = 0; i < 3; i++) {
        if (pthread_create(&metric_threads[i], NULL, request_per_second, &args[i]) != 0) {
            perror("pthread_create for metrics");
        }
    }

    struct epoll_event ev, events[EPOLL_MAX_EVENTS];
    int fd;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("Failed to create socket");
        return;
    }

    /* Set listening socket to non-blocking */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    init_listener(fd, engine);
    int epfd = create_epoll(fd, &ev);
    handle_connections(fd, epfd, &ev, events);

    close(fd);

    for (int i = 0; i < 3; i++) {
        pthread_join(metric_threads[i], NULL);
    }

    return;
}

void process_request(int fd) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    struct http_req *hreq = http_req_init();

    if (handle_request(fd, hreq) != 0) {
        http_req_clean(hreq);
        return;
    }
    
    /* Select backend IMMEDIATELY when request arrives */
    int backend_port = get_next_backend_port();
    
    #if DEBUG
        printf("[DEBUG] Forwarding to port %d (counter: %u)\n", 
               backend_port, atomic_load(&rr_counter));
    #endif
    
    /* Increment the RPS counter for the selected backend port */
    int port_index = backend_port - 3000;
    atomic_fetch_add(&rps_counters[port_index], 1);

    handle_response(fd, hreq, backend_port);
    clock_gettime(CLOCK_MONOTONIC, &end);

    #if DEBUG
        log_elapsed_time(start, end);
    #endif
    
    http_req_clean(hreq);
}

/**
 * Forward request to specific backend port
 * Port is selected in process_request to ensure proper round-robin
 */
int forwards_to_port(const char *request, char *http_response, size_t size, int port) {
    int fd;
    fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        return -1;
    }

    if (connect_to_port(fd, port) != 0) {
        close(fd);
        return -1;
    }

    send(fd, request, strlen(request), 0);
    recv(fd, http_response, size, 0);

    close(fd);
    return 0;
}

/* Keep backwards compatibility */
int forwards(const char *request, char *http_response, size_t size) {
    int port = get_next_backend_port();
    return forwards_to_port(request, http_response, size, port);
}

void destroy_engine(struct prescal_engine *engine) {
    free(engine->config->forwards);
    free(engine->config);
    free(engine);
}