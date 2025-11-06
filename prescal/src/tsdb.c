#include "../include/tsdb.h"
#include "../include/helper.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

struct timeseries_db *init_tsdb(uint16_t port) {
    struct timeseries_db *tsdb =
        malloc(sizeof(struct timeseries_db));
    if (!tsdb) {
        perror("MALLOC");
        exit(EXIT_FAILURE);
    }
    tsdb->port = port;
    tsdb->fd = 0;
    return tsdb;
}

int tsdb_connect(struct timeseries_db *tsdb) {
    struct sockaddr_in server;
    int fd;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket tsdb_connect");
        return ERR_SOCKET;
    }

    server.sin_port = htons(tsdb->port);
    server.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    if (connect(fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect tsdb_connect");
        close(fd);
        return ERR_CONNECT;
    }

    tsdb->fd = fd;
    return CONN_SUCCESS;
}

void tsdb_write(struct timeseries_db *tsdb, char *payload, size_t size) {
    char token[256];
    read_env("TSDB_TOKEN", token);
    char request[2048];
    printf("PAYLOAD: %s\n", payload);

    int len = snprintf(request, sizeof(request),
        "POST /api/v2/write?bucket=prescal&precision=s HTTP/1.1\r\n"
        "Host: localhost:8181\r\n"
        "Authorization: Token %s\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        "%s",
        token, size, payload);

    // Send only the bytes actually written by snprintf
    if (send(tsdb->fd, request, len, 0) < 0) {
        perror("send tsdb_write\n");
        return;
    }

    // Optionally, read response (non-blocking or timeout)
    char buffer[512];
    int bytes = recv(tsdb->fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
    if (bytes > 0) {
        buffer[bytes] = '\0';
    }
}


void tsdb_read(struct timeseries_db *tsdb) {
    struct sockaddr_in db_server;
    char request[2048], buffer[4096], token[256];
    read_env("TSDB_TOKEN", token);

    snprintf(request, sizeof(request),
        "GET /api/v3/query_sql?db=prescal&q=SELECT%%20*%%20FROM%%20server HTTP/1.0\r\n"
        "Host: localhost:8181\r\n"
        "Authorization: Token %s\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n",
        token
    );

    if (send(tsdb->fd, request, strlen(request), 0) < 0) {
        perror("send");
        close(tsdb->fd);
        exit(EXIT_FAILURE);
    }

    memset(buffer, 0, sizeof(buffer));
    int n;
    while ((n = recv(tsdb->fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        printf("%s\n", buffer);
    }

    if (n < 0)
        perror("recv");

    close(tsdb->fd);
}
