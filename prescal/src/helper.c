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