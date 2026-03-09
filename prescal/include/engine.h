#ifndef ENGINE
#define ENGINE

#include "configuration.h"
#include "ds/linkedlist.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_BUFF_SIZE 9000
#define BACKEND_PORT 3000

#define TSDB_ENABLE 1

#define ON_SOCK_ERR                                                            \
  "HTTP/1.1 502 Bad Gateway\r\n"                                               \
  "Content-Type: text/plain\r\n"                                               \
  "Content-Length: 14\r\n"                                                     \
  "\r\n"                                                                       \
  "Socket Failure"

struct prescal_engine {
  struct prescal_config *config;
  struct linkedlist *backends;
  atomic_uint_fast32_t rr_counter;
  pthread_t *metric_threads;
  struct thread_arg *metric_args;
  int num_metric_threads;
  pthread_t logger_thread;
  atomic_bool running;
};

struct thread_arg {
  int port;
  int index;
};

/* Prescal Engine function Definition */
struct prescal_engine *engine_init(struct prescal_config *config);
void start(struct prescal_engine *engine);
void process_request(int fd);
int forwards(const char *request, char *http_response, size_t size);
int forwards_to_port(const char *request, char *http_response, size_t size,
                     int port);
void destroy_engine(struct prescal_engine *engine);

#endif