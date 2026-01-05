#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/config.h"

traffic_config_t* get_default_config(void) {
    traffic_config_t *config = malloc(sizeof(traffic_config_t));
    if (!config) return NULL;

    config->base_min_requests = 50;
    config->base_max_requests = 150;
    config->high_traffic_min_requests = 400;
    config->high_traffic_max_requests = 500;
    config->normal_traffic_duration = 1800; // 30 minutes
    config->high_traffic_duration = 300;    // 5 minutes

    return config;
}

int validate_config(traffic_config_t *config) {
    if (!config) return -1;

    if (config->base_min_requests < 0 ||
        config->base_max_requests < 0 ||
        config->high_traffic_min_requests < 0 ||
        config->high_traffic_max_requests < 0 ||
        config->normal_traffic_duration <= 0 ||
        config->high_traffic_duration <= 0) {
        return -1;
    }

    if (config->base_min_requests >= config->base_max_requests) return -1;
    if (config->high_traffic_min_requests >= config->high_traffic_max_requests) return -1;

    return 0;
}

void free_config(traffic_config_t *config) {
    if (config) {
        free(config);
    }
}

traffic_config_t* load_config(const char *config_path) {
    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        printf("Config file not found at %s, using defaults\n", config_path);
        return get_default_config();
    }

    traffic_config_t *config = malloc(sizeof(traffic_config_t));
    if (!config) {
        fclose(fp);
        return NULL;
    }

    // Initialize with default values before loading
    *config = *get_default_config();

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            int val = atoi(value);

            if (strcmp(key, "base_min_requests") == 0) {
                config->base_min_requests = val;
            } else if (strcmp(key, "base_max_requests") == 0) {
                config->base_max_requests = val;
            } else if (strcmp(key, "high_traffic_min_requests") == 0) {
                config->high_traffic_min_requests = val;
            } else if (strcmp(key, "high_traffic_max_requests") == 0) {
                config->high_traffic_max_requests = val;
            } else if (strcmp(key, "normal_traffic_duration") == 0) {
                config->normal_traffic_duration = val;
            } else if (strcmp(key, "high_traffic_duration") == 0) {
                config->high_traffic_duration = val;
            }
        }
    }

    fclose(fp);

    if (validate_config(config) != 0) {
        printf("Invalid config values, using defaults\n");
        free_config(config);
        return get_default_config();
    }

    return config;
}


