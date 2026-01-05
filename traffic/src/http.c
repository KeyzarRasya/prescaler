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
        // Non-critical, so don't exit, just report
        perror("socket");
        return;
    }

    struct sockaddr_in target = {
        .sin_family = AF_INET,
        .sin_port = htons(80)
    };
    inet_pton(AF_INET, "127.0.0.1", &target.sin_addr);
    
    socklen_t target_size = sizeof(target);
    if (connect(fd, (struct sockaddr*)&target, target_size) < 0) {
        perror("connect");
        close(fd);
        return;
    }

    char request[] = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    char response[MAX_BUFFER_SIZE];
    send(fd, request, strlen(request), 0);
    recv(fd, response, sizeof(response), 0);
    close(fd);
    return;
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
    printf("Preparing to send %d requests...\n", amount);
    while (i < amount) {
        send_request();
        i++;
        
        /* Use thread-local random with staggered delays */
        int delay = (rand_r(&seed) % 15000) + 2000; // 2-17ms random delay
        usleep(delay);
    }
    printf("Request batch done: %d requests sent.\n", amount);
    return NULL;
}

void traffic_worker() {
    /* Load config once at startup */
    traffic_config_t *config = load_config("traffic.conf");
    if (!config) {
        printf("Failed to load config, using defaults\n");
        config = get_default_config();
    }

    if (validate_config(config) != 0) {
        printf("Invalid config, using defaults\n");
        free_config(config);
        config = get_default_config();
    }

    /* Seed master random number generator once at start */
    srand(time(NULL));

    // State management
    enum { STATE_NORMAL, STATE_HIGH } current_state = STATE_NORMAL;
    int seconds_in_current_state = 0;
    
    printf("Starting traffic generation. Initial state: NORMAL period for %d seconds.\n", config->normal_traffic_duration);

    while (1) {
        seconds_in_current_state++;
        int traffic = 0;
        int range = 0;

        if (current_state == STATE_NORMAL) {
            // --- In Normal Period ---
            range = config->base_max_requests - config->base_min_requests + 1;
            traffic = (rand() % range) + config->base_min_requests;

            // Check if it's time to switch to high traffic period
            if (seconds_in_current_state >= config->normal_traffic_duration) {
                current_state = STATE_HIGH;
                seconds_in_current_state = 0; // Reset timer for new state
                printf("Switching to HIGH traffic period for %d seconds.\n", config->high_traffic_duration);
            }

        } else { // current_state == STATE_HIGH
            // --- In High Traffic Period ---
            range = config->high_traffic_max_requests - config->high_traffic_min_requests + 1;
            traffic = (rand() % range) + config->high_traffic_min_requests;

            // Check if it's time to switch back to normal period
            if (seconds_in_current_state >= config->high_traffic_duration) {
                current_state = STATE_NORMAL;
                seconds_in_current_state = 0; // Reset timer for new state
                printf("Switching to NORMAL traffic period for %d seconds.\n", config->normal_traffic_duration);
            }
        }

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
        
        // Sleep for 1 second before the next cycle
        usleep(1000 * 1000);
    }

    // Cleanup (never reached in current implementation)
    free_config(config);
    return;
}


