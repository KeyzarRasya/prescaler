#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include "../include/http.h"
#include "../include/config.h"
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void send_request(void) {
    int fd;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in target = {
        .sin_family = AF_INET,
        .sin_port = htons(80)
    };
    inet_pton(AF_INET, "127.0.0.1", &target.sin_addr);
    
    socklen_t target_size = sizeof(target);
    if (connect(fd, (struct sockaddr*)&target, target_size) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    char request[] = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    char response[MAX_BUFFER_SIZE];
    send(fd, request, strlen(request), 0);
    int n = recv(fd, response, sizeof(response), 0);
    close(fd);
    return;
}

int is_in_period(int hour, time_period_t *period) {
    if (period->start_hour <= period->end_hour) {
        // Normal case: 8:00 - 17:00
        return hour >= period->start_hour && hour < period->end_hour;
    } else {
        // Wraparound case: 22:00 - 08:00
        return hour >= period->start_hour || hour < period->end_hour;
    }
}

int traffic_amount_v2(struct tm *localtime, traffic_config_t *config) {
    int hour = localtime->tm_hour;
    time_period_t *period = NULL;

    // Determine which period we're in
    if (is_in_period(hour, &config->business)) {
        period = &config->business;
    } else if (is_in_period(hour, &config->evening)) {
        period = &config->evening;
    } else if (is_in_period(hour, &config->night)) {
        period = &config->night;
    } else {
        // Fallback - shouldn't happen if config is valid
        return 100;
    }

    // Generate random in range [min, max]
    int range = period->max_requests - period->min_requests + 1;
    return (rand() % range) + period->min_requests;
}

/**
 * Thread-safe random number generation
 * Uses thread ID and high-resolution time for unique seed per thread
 */
static unsigned int get_thread_seed(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned int seed = (unsigned int)(ts.tv_nsec ^ (ts.tv_sec << 16));
    seed ^= (unsigned int)pthread_self();
    return seed;
}

void* traffic_forwarder(void *args) {
    int amount = *(int *)args;
    free(args);
    
    /* Each thread gets its own random seed */
    unsigned int seed = get_thread_seed();
    
    int i = 0;
    printf("Preparing sending %d request\n", amount);
    while (i < amount) {
        send_request();
        i++;
        
        /* Use thread-local random with staggered delays */
        int delay = (rand_r(&seed) % 15000) + 2000; // 2-17ms random delay
        usleep(delay);
    }
    printf("Request Done: %d Requests\n", amount);
    return NULL;
}

void traffic_worker() {
    /* Load config once at startup */
    traffic_config_t *config = load_config("/root/traffic.conf");
    if (!config) {
        printf("Failed to load config, using defaults\n");
        config = get_default_config();
    }

    if (validate_config(config) != 0) {
        printf("Invalid config, using defaults\n");
        free_config(config);
        config = get_default_config();
    }

    /* Seed once at start */
    srand(time(NULL));

    while (1) {
        time_t now = time(NULL);
        struct tm *local_time = localtime(&now);
        int traffic = traffic_amount_v2(local_time, config);

        int *traffic_copy = malloc(sizeof(int));
        if (!traffic_copy) {
            perror("malloc failed");
            continue;
        }
        *traffic_copy = traffic;
        pthread_t forwarder;
        if (pthread_create(&forwarder, NULL, traffic_forwarder, traffic_copy) != 0) {
            perror("pthread_create failed");
            free(traffic_copy);
            continue;
        }
        pthread_detach(forwarder);
        usleep(1000 * 1000);
    }

    // Cleanup (never reached in current implementation)
    free_config(config);
    return;
}
