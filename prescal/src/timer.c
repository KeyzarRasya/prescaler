#include "../include/timer.h"
#include <unistd.h>
#include <time.h>
#include <string.h>

void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void get_timestamp(char *timestr, size_t size) {
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    strftime(timestr, size, "%a %b %d %H:%M:%S %Y", local_time);
}