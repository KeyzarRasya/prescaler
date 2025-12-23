#ifndef HTTP
#define HTTP

#include "config.h"
#include <time.h>

#define MAX_BUFFER_SIZE 9000
#define HOST "localhost:80"

void prescal_traffic();
void send_request();
void traffic_worker();
int traffic_amount_v2(struct tm *localtime, traffic_config_t *config);
int is_in_period(int hour, time_period_t *period);

#endif