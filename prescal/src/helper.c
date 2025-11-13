#include "../include/helper.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int split_equal(const char *input, char **left, char **right) {
    const char *equal_sign = strchr(input, '=');
    if (!equal_sign) {
        return -1;
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

int read_env(const char *to_search, char *dest, size_t dest_size) {
    FILE *fptr;
    char buffer[256];
    const char *home = getenv("HOME");
    char env_path[512];

    if (!home) {
        fprintf(stderr, "HOME not set.\n");
        return -1;
    }

    snprintf(env_path, sizeof(env_path), "%s/.prescal/.env", home);    
    fptr = fopen(env_path, "r");
    if (fptr == NULL) {
        perror("ERR");
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), fptr)) {
        buffer[strcspn(buffer, "\r\n")] = 0;

        char *equal = strchr(buffer, '=');
        if (!equal) continue;

        *equal = '\0';
        const char *key = buffer;
        const char *value = equal + 1;

        if (strcmp(to_search, key) == 0) {
            strncpy(dest, value, dest_size - 1);
            dest[dest_size - 1] = '\0';
            fclose(fptr);
            return 1;
        }
    }

    fclose(fptr);
    return -1;
}