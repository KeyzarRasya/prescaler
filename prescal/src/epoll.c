#include <sys/epoll.h>
#include "../include/epoll.h"

int create_epoll(int server_fd, struct epoll_event *ev) {
    int epfd = epoll_create1(0);
    
    if (epfd == -1) {
        return -1;
    }
    
    // Initialize the epoll event structure
    ev->events = EPOLLIN | EPOLLET;
    ev->data.fd = server_fd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, ev) == -1) {
        return -1;
    }

    return epfd;
}