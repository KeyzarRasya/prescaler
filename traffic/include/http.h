#ifndef HTTP
#define HTTP

#include "config.h"
#include <time.h>

#define MAX_BUFFER_SIZE 9000
#define HOST "localhost:80"
#define MAX_CONCURRENT_THREADS 4

void send_request();
void traffic_worker();

#endif