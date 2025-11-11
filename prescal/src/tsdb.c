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
    char host[126];
    read_env("TSDB_TOKEN", token, sizeof(token));
    read_env("TSDB_HOST", host, sizeof(host));

    char request[2048];
    printf("PAYLOAD: %s\n", payload);

    snprintf(request, sizeof(request),
        "POST /api/v2/write?org=docs&bucket=prescal&precision=s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Authorization: Token %s\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        host, token, strlen(payload), payload);

    if (send(tsdb->fd, request, strlen(request), 0) < 0) {
        perror("send tsdb_write");
        return;
    }

    char buffer[512];
    int bytes = recv(tsdb->fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Response: %s\n", buffer);
    }
}


void tsdb_read(struct timeseries_db *tsdb) {
    struct sockaddr_in db_server;
    char request[2048], buffer[4096], token[256], host[126];
    read_env("TSDB_TOKEN", token, sizeof(token));
    read_env("TSDB_HOST", host, sizeof(host));

    const char *body =
        "{\"query\":\"from(bucket: \\\"prescal\\\") |> range(start: -1h) |> "
        "filter(fn: (r) => r._measurement == \\\"server\\\") |> "
        "pivot(rowKey:[\\\"_time\\\"], columnKey:[\\\"_field\\\"], valueColumn:\\\"_value\\\")\"}";

    int body_len = strlen(body);

    snprintf(request, sizeof(request),
        "POST /api/v2/query?org=docs HTTP/1.0\r\n"
        "Host: %s\r\n"
        "Authorization: Token %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        host, token, body_len, body);

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
