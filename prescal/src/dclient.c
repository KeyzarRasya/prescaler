#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include "../include/dclient.h"

int dclient_connect(void) {
    int fd;
    struct sockaddr_un addr;
    fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("Failed to create a socket");
        exit(EXIT_FAILURE);        
    }
    memset(&addr, 0, sizeof(struct sockaddr_un));

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, DOCKER_SOCK);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Failed to connect to Docker Socket");
        exit(EXIT_FAILURE);
    }
   return fd;
}

void dclient_new_instance(int fd) {
    char buffer[9000];

    char *body = 
    "{\"Image\":\"test-web\",\"Env\":[\"PORT=3000\"],\"Cmd\":[\"npm\",\"run\",\"dev\"]}";
    char request[1024];
    snprintf(request, sizeof(request),
        "POST /containers/create HTTP/1.1\r\n"
        "Content-Type:application/json\r\n"
        "Host: Docker\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n"
        "%s", strlen(body), body
    );

    if (send(fd, request, strlen(request), 0) < 0) {
        perror("docker send");
        close(fd);
        exit(EXIT_FAILURE);
    }

    int n = read(fd, buffer, sizeof(buffer));
    if (n < 0) {
        perror("read");
        close(fd);
        return;
    }
    buffer[n] = '\0';
    printf("%s\n", buffer);
}

void dclient_start_container(int fd) {
    char buffer[1024];

    char *id = "2f5d65c47377bdd93f1dc1b7bea7c93f42496d2348f76fa8f66d3181e27aa45e";

    char request[1024];
    snprintf(request, sizeof(request), 
        "POST /containers/%s/start HTTP/1.1\r\n"
        "Host: Docker\r\n"
        "Connection: close\r\n\r\n",
        id
    );

    if (send(fd, request, sizeof(request), 0) < 0) {
        perror("send container");
        close(fd);
        exit(EXIT_FAILURE);
    }

    int n = read(fd, buffer, sizeof(buffer));
    if (n < 0) {
        perror("read");
        return;
    }
    buffer[n] = '\0';
    printf("\n%s\n", buffer);

}