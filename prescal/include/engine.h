#ifndef ENGINE
#define ENGINE

#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include "configuration.h"
#include "ds/linkedlist.h"

#define MAX_BUFF_SIZE   9000
#define BACKEND_PORT    3000

#define TSDB_ENABLE 1

#define ON_SOCK_ERR \
    "HTTP/1.1 502 Bad Gateway\r\n" \
    "Content-Type: text/plain\r\n" \
    "Content-Length: 14\r\n" \
    "\r\n" \
    "Socket Failure"

struct prescal_engine{
    struct prescal_config *config;
    struct linkedlist *backends;
    atomic_uint_fast32_t rr_counter;
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
int forwards_to_port(const char *request, char *http_response, size_t size, int port);
void destroy_engine(struct prescal_engine *engine);


#endif