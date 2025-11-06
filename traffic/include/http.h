#ifndef HTTP
#define HTTP

#define MAX_BUFFER_SIZE 9000
#define HOST "localhost:80"

void prescal_traffic();
void send_request();
void traffic_worker();

#endif