#ifndef TSDB
#define TSDB

#include <stdint.h>
#include <stddef.h>

#define CONN_SUCCESS    0
#define ERR_SOCKET      -1
#define ERR_CONNECT     -2

struct timeseries_db {
    uint16_t port;
    char *host;
    char *database;
    char *table;
    int fd;
};

struct timeseries_db *init_tsdb(uint16_t port);
int tsdb_connect(struct timeseries_db *tsdb);
void tsdb_write(struct timeseries_db *tsdb, char *payload, size_t size);
void write_db(struct timeseries_db tsdb, char *data);
void tsdb_read(struct timeseries_db *tsdb);

#endif