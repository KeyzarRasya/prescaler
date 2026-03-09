#define _POSIX_C_SOURCE 200809L
#include "../include/engine.h"
#include "../include/epoll.h"
#include "../include/http.h"
#include "../include/metrics.h"
#include "../include/timer.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DEBUG 0

// Global engine instance
static struct prescal_engine *g_engine;

/* Per-port RPS counters - dynamically allocated */
static _Atomic(atomic_int *) rps_counters_ptr;
static atomic_int rps_counters_size = 0;

double cpu_t0;

/* Internal functions for dynamic scaling */
static void *watch_config(void *arg);
static void reload_config(struct prescal_engine *engine);
static void update_rps_counters(int new_size);
static void start_metric_threads(struct prescal_engine *engine);
static void stop_metric_threads(struct prescal_engine *engine);
static void *engine_logger(void *arg);

void *request_per_second(void *arg) {
  struct thread_arg *targ = (struct thread_arg *)arg;
  int port = targ->port;
  int index = targ->index;
  struct timeseries_db *tsdb = init_tsdb(8086);
  if (tsdb_connect(tsdb) != CONN_SUCCESS) {
    perror("tsdb start");
    return NULL;
  }
  tsdb->table = "server";

  char filename[32];
  snprintf(filename, sizeof(filename), "data_%d.txt", port);
  fprintf(stderr, "INFO: Attempting to open data file: %s\n", filename);
  FILE *fptr = fopen(filename, "a");
  if (!fptr) {
    perror("file pointer");
    return NULL;
  }

  char metrics_res[MAX_METRICS_RESPONSE];

  // RESILIENCE: Retry initial connection until successful or thread stopped
  // This is crucial because containers take time to start their metrics API
  while (atomic_load(&g_engine->running)) {
    if (request_metrics(port, metrics_res, sizeof(metrics_res)) == 0) {
      break;
    }
    // fprintf(stderr, "DEBUG: Waiting for port %d metrics API to be
    // ready...\n", port);
    sleep_ms(1000);
  }

  if (!atomic_load(&g_engine->running)) {
    fclose(fptr);
    return NULL;
  }

  double cpu_t0 = get_value(metrics_res, CPU_SECOND);

  // Enable cancellation for this thread
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL); // Safe cancellation

  while (atomic_load(&g_engine->running)) {
    pthread_testcancel(); // Cancellation point
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
    // Safety check: ensure rps_counters pointer and index are valid for the
    // current configuration
    atomic_int *current_counters = atomic_load(&rps_counters_ptr);
    int rps = 0;
    if (current_counters && index < atomic_load(&rps_counters_size)) {
      rps = atomic_exchange(&current_counters[index], 0);
    }
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
    /* Don't close(fd) here — caller (handle_connections) is responsible */
    return -1;
  }
  request[n] = '\0';
  convert_request(hreq, request, sizeof(request));
  return 0;
}

static void handle_response(int fd, struct http_req *hreq, int backend_port) {
  char response[MAX_BUFF_SIZE];
  if (forwards_to_port(hreq->raw, response, sizeof(response), backend_port) !=
      0) {
    snprintf(response, sizeof(response), "%s", ON_SOCK_ERR);
  }
  send(fd, response, strlen(response), 0);
}

void log_elapsed_time(struct timespec start, struct timespec end) {
  double elapsed =
      (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
  printf("Elapsed time: %0.6f\n", elapsed);
}

static int get_next_backend_port(int *out_port, int *out_index) {
  if (!g_engine || !g_engine->config) {
    return -1;
  }

  pthread_mutex_lock(&g_engine->config->mutex);

  if (!g_engine->backends || g_engine->backends->size == 0) {
    pthread_mutex_unlock(&g_engine->config->mutex);
    return -1; // No backends configured
  }

  uint32_t current = atomic_fetch_add(&g_engine->rr_counter, 1);
  int index = current % g_engine->backends->size;

  struct node *backend_node = get_node_at(g_engine->backends, index);
  if (!backend_node) {
    pthread_mutex_unlock(&g_engine->config->mutex);
    return -1; // Should not happen
  }

  // Parse port from "host:port" string
  char *colon = strrchr(backend_node->value, ':');
  if (!colon) {
    pthread_mutex_unlock(&g_engine->config->mutex);
    return -1; // Invalid format
  }

  *out_port = atoi(colon + 1);
  *out_index = index;

  pthread_mutex_unlock(&g_engine->config->mutex);
  return 0;
}

/**
 * Connect to specific backend server port
 * This version takes the port as a parameter for better control
 */
static int connect_to_port(int fd, int port) {
  struct sockaddr_in destiny = {.sin_family = AF_INET, .sin_port = htons(port)};
  inet_pton(AF_INET, "127.0.0.1", &destiny.sin_addr);

  if (connect(fd, (struct sockaddr *)&destiny, sizeof(destiny)) < 0) {
    return -1;
  }
  return 0;
}

void init_listener(int fd, struct prescal_engine *engine) {
  struct sockaddr_in server = {.sin_family = AF_INET,
                               .sin_port = htons(engine->config->port),
                               .sin_addr = INADDR_ANY};

  int enable = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
    perror("socket option");
    close(fd);
    exit(EXIT_FAILURE);
  }

  if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
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

void handle_connections(int fd, int epfd, struct epoll_event *ev,
                        struct epoll_event *events) {
  struct sockaddr_in client;
  printf("Listening on port %d...\n", g_engine->config->port);

  while (1) {
    int npfd = epoll_wait(epfd, events, EPOLL_MAX_EVENTS, -1);
    for (int i = 0; i < npfd; i++) {
      if (events[i].data.fd == fd) {
        /* Accept all pending connections in this iteration */
        while (1) {
          socklen_t client_size = sizeof(client);
          int new_fd = accept(fd, (struct sockaddr *)&client, &client_size);
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
          ev->events =
              EPOLLIN; /* Level-triggered: safer for single recv() pattern */
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

struct prescal_engine *engine_init(struct prescal_config *config) {
  g_engine = malloc(sizeof(struct prescal_engine));
  if (!g_engine) {
    perror("Failed to allocate engine");
    return NULL;
  }

  g_engine->config = config;
  g_engine->backends = config->forwards; // Use the list from config
  atomic_init(&g_engine->rr_counter, 0);

  // Allocate and initialize RPS counters
  if (g_engine->backends->size > 0) {
    int initial_size = g_engine->backends->size;
    atomic_int *initial_counters = calloc(initial_size, sizeof(atomic_int));
    if (!initial_counters) {
      perror("Failed to allocate rps_counters");
      free(g_engine);
      return NULL;
    }
    for (int i = 0; i < initial_size; i++) {
      atomic_init(&initial_counters[i], 0);
    }
    atomic_store(&rps_counters_ptr, initial_counters);
    atomic_store(&rps_counters_size, initial_size);
  } else {
    atomic_store(&rps_counters_ptr, NULL);
    atomic_store(&rps_counters_size, 0);
  }

  g_engine->metric_threads = NULL;
  g_engine->metric_args = NULL;
  g_engine->num_metric_threads = 0;
  atomic_init(&g_engine->running, true);

  return g_engine;
}

static void stop_metric_threads(struct prescal_engine *engine) {
  if (engine->metric_threads) {
    // We only cancel the threads, we don't set running=false here
    // because reload might want to keep the engine running.
    for (int i = 0; i < engine->num_metric_threads; i++) {
      pthread_cancel(engine->metric_threads[i]);
      pthread_join(engine->metric_threads[i], NULL);
    }
    free(engine->metric_threads);
    free(engine->metric_args);
    engine->metric_threads = NULL;
    engine->metric_args = NULL;
    engine->num_metric_threads = 0;
  }
}

static void start_metric_threads(struct prescal_engine *engine) {
  int num_backends = engine->backends->size;
  if (num_backends == 0)
    return;

  engine->metric_threads = malloc(sizeof(pthread_t) * num_backends);
  engine->metric_args = malloc(sizeof(struct thread_arg) * num_backends);
  engine->num_metric_threads = num_backends;

  struct node *current = engine->backends->first;
  for (int i = 0; i < num_backends; i++) {
    if (!current)
      break;
    char *colon = strrchr(current->value, ':');
    engine->metric_args[i].port = colon ? atoi(colon + 1) : 0;
    engine->metric_args[i].index = i;

    if (pthread_create(&engine->metric_threads[i], NULL, request_per_second,
                       &engine->metric_args[i]) != 0) {
      perror("pthread_create for metrics");
    }
    current = current->next;
  }
}

static void *engine_logger(void *arg) {
  struct prescal_engine *engine = (struct prescal_engine *)arg;
  FILE *log_fp = fopen("prescal.log", "a");
  if (!log_fp) {
    perror("Could not open prescal.log");
    return NULL;
  }

  fprintf(stderr, "INFO: Per-second engine logger started.\n");

  while (atomic_load(&engine->running)) {
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));

    pthread_mutex_lock(&engine->config->mutex);
    int num_backends = engine->backends->size;

    fprintf(log_fp, "[%s] Active Backends: %d | distribution: [", timestamp,
            num_backends);

    for (int i = 0; i < num_backends; i++) {
      struct node *backend_node = get_node_at(engine->backends, i);
      if (!backend_node)
        continue;

      char *colon = strrchr(backend_node->value, ':');
      int port = colon ? atoi(colon + 1) : 0;

      // Get current value from rps_counters safely
      atomic_int *current_counters = atomic_load(&rps_counters_ptr);
      int rps_val = 0;
      if (current_counters && i < atomic_load(&rps_counters_size)) {
        rps_val = atomic_load(&current_counters[i]);
      }

      fprintf(log_fp, "port %d: %d rps%s", port, rps_val,
              (i == num_backends - 1) ? "" : ", ");
    }
    fprintf(log_fp, "]\n");
    fflush(log_fp);
    pthread_mutex_unlock(&engine->config->mutex);

    sleep_ms(1000);
  }

  fclose(log_fp);
  return NULL;
}

static void update_rps_counters(int new_size) {
  if (new_size == atomic_load(&rps_counters_size))
    return;

  atomic_int *new_counters = calloc(new_size, sizeof(atomic_int));
  if (!new_counters) {
    perror("Failed to allocate new rps_counters");
    return;
  }

  for (int i = 0; i < new_size; i++) {
    atomic_init(&new_counters[i], 0);
  }

  // Atomically swap the pointer and update the size
  atomic_int *old_counters = atomic_exchange(&rps_counters_ptr, new_counters);
  atomic_store(&rps_counters_size, new_size);

  // Note: We'd use a grace period (RCU-style) here if this were extremely
  // performance critical. For now, we'll delay deletion slightly or accept that
  // joining monitor threads will clear it.
  if (old_counters) {
    // Give existing requests a tiny bit of time to finish before free
    // In a production system, we'd wait for a grace period.
    sleep_ms(100);
    free(old_counters);
  }
}

static void reload_config(struct prescal_engine *engine) {
  fprintf(stderr, "INFO: Reloading configuration...\n");

  pthread_mutex_lock(&engine->config->mutex);

  // Stop existing monitoring threads before updating backend list
  stop_metric_threads(engine);

  // Clear old forwards (simplified: we should ideally compare and only
  // add/remove) But for now, we re-read everything.
  struct linkedlist *old_forwards = engine->config->forwards;
  engine->config->forwards = linkedlist_init();

  read_config(engine->config, PATH);
  engine->backends = engine->config->forwards;

  int new_size = engine->backends->size;
  update_rps_counters(new_size);

  // Restart monitoring for the new backend list
  start_metric_threads(engine);

  pthread_mutex_unlock(&engine->config->mutex);

  fprintf(stderr, "INFO: Configuration reloaded. New backend count: %d\n",
          new_size);
}

static void *watch_config(void *arg) {
  struct prescal_engine *engine = (struct prescal_engine *)arg;
  int fd = inotify_init();
  if (fd < 0) {
    perror("inotify_init");
    return NULL;
  }

  int wd = inotify_add_watch(fd, PATH, IN_MODIFY);
  if (wd < 0) {
    perror("inotify_add_watch");
    close(fd);
    return NULL;
  }

  char buffer[4096];
  while (1) {
    int length = read(fd, buffer, sizeof(buffer));
    if (length < 0) {
      perror("read inotify");
      break;
    }

    int i = 0;
    while (i < length) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];
      if (event->mask & IN_MODIFY) {
        // Give it a small delay to ensure file write is complete
        sleep_ms(100);
        reload_config(engine);
      }
      i += sizeof(struct inotify_event) + event->len;
    }
  }

  inotify_rm_watch(fd, wd);
  close(fd);
  return NULL;
}

void start(struct prescal_engine *engine) {
  pthread_t watch_thread;
  if (pthread_create(&watch_thread, NULL, watch_config, engine) != 0) {
    perror("pthread_create for config watcher");
  }

  // Initialize and start metric threads
  start_metric_threads(engine);

  // Start per-second engine logger
  if (pthread_create(&engine->logger_thread, NULL, engine_logger, engine) !=
      0) {
    perror("pthread_create for engine logger");
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

  // Cleanup threads on exit
  atomic_store(&engine->running, false);
  stop_metric_threads(engine);
  pthread_join(engine->logger_thread, NULL);

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

  int backend_port, backend_index;
  if (get_next_backend_port(&backend_port, &backend_index) != 0) {
    // Handle error: no backend available
    http_req_clean(hreq);
    // Maybe send a 503 Service Unavailable response
    return;
  }

#if DEBUG
  printf("[DEBUG] Forwarding to port %d (index: %d)\n", backend_port,
         backend_index);
#endif

  /* Increment the RPS counter for the selected backend port */
  int current_size = atomic_load(&rps_counters_size);
  atomic_int *current_counters = atomic_load(&rps_counters_ptr);
  if (current_counters && backend_index < current_size) {
    atomic_fetch_add(&current_counters[backend_index], 1);
  }

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
int forwards_to_port(const char *request, char *http_response, size_t size,
                     int port) {
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
  int port, index;
  if (get_next_backend_port(&port, &index) == 0) {
    return forwards_to_port(request, http_response, size, port);
  }
  return -1; // No backend available
}

void destroy_engine(struct prescal_engine *engine) {
  if (!engine)
    return;

  config_destroy(engine->config);
  atomic_int *final_counters = atomic_load(&rps_counters_ptr);
  free(final_counters);
  free(engine->metric_threads);
  free(engine->metric_args);
  free(engine);
}
