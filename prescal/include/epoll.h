#ifndef EPOLL
#define EPOLL

#include <sys/epoll.h>

#define EPOLL_MAX_EVENTS 128

int create_epoll(int server_fd, struct epoll_event *ev);

#endif