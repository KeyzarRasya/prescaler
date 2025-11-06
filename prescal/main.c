#define _POSIX_C_SOURCE 200809L
#include "include/http.h"
#include "include/dclient.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/engine.h"
#include "include/configuration.h"
#include "include/ds/linkedlist.h"
#include <time.h>


void linkedlist_test(void){
    struct linkedlist *ll = linkedlist_init();
    append_node(ll, "localhost:3000");
    append_node(ll, "localhost:3001");
    append_node(ll, "localhost:3002");
    print_ll(ll);
}

void start_engine(void) {
    struct prescal_engine *engine = engine_init("127.0.0.1", 80);
    start(engine);

    destroy_engine(engine);
}

void test_config(void) {
    struct prescal_config *config = config_init();
    read_config(config);
    print_config(config);
}

void docker_instance(void) {
    int docker_fd = dclient_connect();
    // dclient_new_instance(docker_fd);
    dclient_start_container(docker_fd);
    close(docker_fd);
}

int split_equal(const char *input, char **left, char **right) {
    const char *equal_sign = strchr(input, '=');
    if (!equal_sign) {
        return -1;  // '=' not found
    }

    size_t left_len = equal_sign - input;
    size_t right_len = strlen(equal_sign + 1);

    *left = malloc(left_len + 1);
    *right = malloc(right_len + 1);

    if (!*left || !*right) {
        perror("Memory allocation failed");
        return -1;
    }

    strncpy(*left, input, left_len);
    (*left)[left_len] = '\0';

    strcpy(*right, equal_sign + 1);

    return 0;
}

int read_env(const char *to_search, char *dest) {
    FILE *fptr;
    char buffer[256];

    fptr = fopen("../.env", "r");
    if (fptr == NULL) {
        perror("ERR");
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), fptr)) {
        char *key, *value;
        split_equal(buffer, &key, &value);
        if (strcmp(to_search, key) == 0) {
            strcpy(dest, value);
            return 1;
        }
    }

    fclose(fptr);
    return -1;
}

int main() {
    start_engine();
    return 0;
}