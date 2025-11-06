#ifndef ENGINE
#define ENGINE

#include <stdint.h>
#include <stddef.h>
#include "configuration.h"

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
    char *host;
    uint16_t port;
    struct prescal_config *config;
};

struct thread_arg {
    int port;
};

/* Prescal Engine function Definition */
struct prescal_engine *engine_init(char *host, uint16_t port);
void start(struct prescal_engine *engine);
void process_request(int fd);
int forwards(const char *request, char *http_response, size_t size);
int forwards_to_port(const char *request, char *http_response, size_t size, int port);
void destroy_engine(struct prescal_engine *engine);

#endif