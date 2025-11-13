#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include "../include/metrics.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include "../include/timer.h"

char *METRICS_HTTP_REQ = 
    "GET /metrics HTTP/1.1\r\n"
    "Host: 127.0.0.1:3000\r\n"
    "Connection: close\r\n"
    "\r\n";

/* Independent Function */
int connect_target(int fd, uint16_t port) {
    struct sockaddr_in target = {
        .sin_family = AF_INET,
        .sin_port = htons(port)  // FIXED: Use the actual port parameter!
    };
    inet_pton(AF_INET, "127.0.0.1", &target.sin_addr);
    return connect(fd, (struct sockaddr*)&target, sizeof(target));
}
/* END */

/* Header defined function */

struct metrics *init_metrics() {
    struct metrics *metrics = 
        malloc(sizeof(struct metrics));
    if (!metrics) {
        perror("metrics malloc");
        exit(EXIT_FAILURE);
    }
    metrics->rps = 0;
    metrics->port = 3000;
    return metrics;
}

int request_metrics(uint16_t port, char *out, size_t size) {
    int fd;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket\n");
        return -1;
    }

    if (connect_target(fd, port) < 0) {
        perror("Cannot connect to target instance\n");
        return -1;
    }

    send(fd, METRICS_HTTP_REQ, strlen(METRICS_HTTP_REQ), 0);
    
    int n = recv(fd, out, size, 0);
    if (n < 0) {
        perror("recv\n");
        return -1;
    }
    out[n] = '\0';
    close(fd);
    return 0;
}

int get_value_metrics(
    const char *metrics_res, 
    char *key, 
    char *value, 
    size_t value_size
) {
    char line[MAX_LINE];
    int line_index = 0;
    int is_hash = 0;

    size_t len = strlen(metrics_res);
    for (size_t i = 0; i < len; i++) {
        if (metrics_res[i] == '#') { is_hash = 1; continue; }
        if (is_hash && metrics_res[i] == '\n') { is_hash = 0; continue; }

        if (!is_hash) {
            if (metrics_res[i] == '\n') {
                line[line_index] = '\0';
                char *token = strtok(line, " ");
                if (token && strcmp(token, key) == 0) {
                    token = strtok(NULL, " ");
                    if (token) {
                        copy_string(token, value, value_size);
                        return 0;
                        break;
                    }
                }
                line_index = 0;
                continue;
            }
            if (line_index < sizeof(line) - 1) {
                line[line_index++] = metrics_res[i];
            }
        }
    }
    return -1;
}

double cpu_percentages(double t0, double t1, double seconds_diff, int cores) {
    if (seconds_diff <= 0 && cores <= 0) return 0.0;
    double delta_t =  t1 - t0;
    if (delta_t < 0) delta_t = 0;
    return (delta_t / (seconds_diff * cores)) * 100;
}

double mem_to_mb(double mem_usage) {
    return mem_usage / (1024 * 1024);
}

double get_value(char *metrics_res, char *key) {
    char *body = strstr(metrics_res, "\r\n\r\n");
    char value[MAX_VALUE_LEN];
    if (body) {
        body += 4;
        if (get_value_metrics(body, key, value, sizeof(value)) != 0) {
            perror("metrics\n");
            return 0.0;
        }
    }
    return strtod(value, NULL);
}

void store_metrics(struct metrics *metrics, char *src) {
    metrics->cpu_usage = get_value(src, CPU_SECOND);
    metrics->mem_usage = get_value(src, MEM_USAGE);
}

void calculate_metrics(struct metrics *metrics, double cpu_t0) {
    metrics->cpu_usage = cpu_percentages(cpu_t0, metrics->cpu_usage, 1, CORES);
    metrics->mem_usage = mem_to_mb(metrics->mem_usage);
}

void write_metrics(FILE *file, struct metrics *metrics, int rps) {
    char timestr[64];
    get_timestamp(timestr, sizeof(timestr));
    fprintf(file, "%s,%d,%f,%f\n", timestr, rps, metrics->cpu_usage, metrics->mem_usage);
    fflush(file);
}

void write_metrics_db(struct timeseries_db *tsdb, struct metrics *metrics) {
    char payloads[512];

    snprintf(payloads, sizeof(payloads),
             "%s,port=%d rps=%di,cpu=%.2f",
             tsdb->table,
             metrics->port,
             metrics->rps,
             metrics->cpu_usage);
    tsdb_write(tsdb, payloads, strlen(payloads));
}



void copy_string(const char *src, char *dest, size_t size) {
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
}