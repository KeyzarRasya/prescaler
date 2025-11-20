#define _POSIX_C_SOURCE 200809L
#include "../include/configuration.h"
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

struct prescal_config *config_init(void) {
    struct prescal_config *config = 
        malloc(sizeof(struct prescal_config));
    
    if (!config) {
        perror("Failed to allocate config");
        exit(EXIT_FAILURE);
        return NULL;
    }

    config->forwards = linkedlist_init();
    if (!config->forwards) {
        perror("Failed to allocate linkedlist");
        exit(EXIT_FAILURE);
        return NULL;
    }
    return config;
}

void trim(char *str) {
    char *start = str;
    while (isspace((unsigned char)*start)) start++;
    memmove(str, start, strlen(start) + 1);

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) *end-- = '\0';
}

void read_config(struct prescal_config *config) {
    const char *home = getenv("HOME");
    if (!home) {
        perror("Cannot read HOME");
        return;
    }
    char path[126];
    snprintf(path, sizeof(path), "%s/prescal/config.yml", home);
    printf("%s\n", path);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("Gagal membuka file");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int parsing_forwards = 0;

    while (fgets(line, sizeof(line), fp)) {
        trim(line);

        if (strlen(line) == 0) continue; 

        if (parsing_forwards && line[0] == '-') {
            char *value = line + 1;
            trim(value);
            append_node(config->forwards, strdup(value));
            continue;
        }

        char *colon = strchr(line, ':');
        if (!colon) continue;

        *colon = '\0';
        char *key = line;
        char *value = colon + 1;
        trim(key);
        trim(value);

        if (strcmp(key, "port") == 0) {
            config->port = (uint16_t)atoi(value);
            parsing_forwards = 0;
        } else if (strcmp(key, "entry") == 0) {
            config->entry = strdup(value);
            parsing_forwards = 0;
        } else if (strcmp(key, "forwards") == 0) {
            parsing_forwards = 1;
        }
    }

    fclose(fp);
}

void print_config(struct prescal_config *config) {
    printf("port = %u\n", config->port);
    printf("entry = %s\n", config->entry);
    printf("forwards:\n");
    struct node *curr = config->forwards->first;

    while (curr) {
        printf("%s\n",curr->value);
        curr = curr->next;
    }
}