#ifndef METRICS
#define METRICS

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "tsdb.h"

#define CORES 1

#define CPU_SECOND              "process_cpu_seconds_total"
#define PROC_CPU_SYSEC_TOTAL    "process_cpu_system_seconds_total"
#define MAX_FDS                 "process_max_fds"
#define NODEJS_HEAP             "nodejs_heap_size_total_bytes"
#define MEM_USAGE               "process_resident_memory_bytes"

#define MAX_METRICS_RESPONSE 9000
#define MAX_LINE        1024
#define MAX_VALUE_LEN   255

struct metrics {
    double mem_usage;
    double cpu_usage;
    int rps;
    uint16_t port;
    time_t timestamp;
};

struct metrics *init_metrics();
int request_metrics(uint16_t port ,char *out, size_t size1);
int get_value_metrics(const char *metrics_res, char *key, char *value, size_t value_size);
double get_value(char *metrics, char *key);
double cpu_percentages(double t0, double t1, double diff, int cores);
double mem_to_mb(double mem_usage);
void copy_string(const char *src, char *out, size_t size);
void store_metrics(struct metrics *metrics, char *src);
void calculate_metrics(struct metrics *metrics, double cpu_t0);
void write_metrics(FILE *file, struct metrics *metrics, int rps);
void write_metrics_db(struct timeseries_db *tsdb, struct metrics *metricse);
void write_metrics_csv();

#endif