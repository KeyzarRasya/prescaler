#ifndef DCLIENT
#define DCLIENT

#define DOCKER_SOCK "/var/run/docker.sock"

#define POST    "POST"
#define GET     "GET"

#define CTN_LIST    "/containers/json"

int dclient_connect(void);
void dclient_send(int fd, char *request);
void dclient_new_instance(int fd);
void dclient_start_container(int fd);

#endif