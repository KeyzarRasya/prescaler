#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/config.h"

traffic_config_t* get_default_config(void) {
    traffic_config_t *config = malloc(sizeof(traffic_config_t));
    if (!config) return NULL;

    // Business hours: 8:00 - 17:00, 300-500 requests
    config->business.start_hour = 8;
    config->business.end_hour = 17;
    config->business.min_requests = 300;
    config->business.max_requests = 500;

    // Evening hours: 17:00 - 22:00, 100-250 requests
    config->evening.start_hour = 17;
    config->evening.end_hour = 22;
    config->evening.min_requests = 100;
    config->evening.max_requests = 250;

    // Night hours: 22:00 - 8:00, 50-150 requests
    config->night.start_hour = 22;
    config->night.end_hour = 8;
    config->night.min_requests = 50;
    config->night.max_requests = 150;

    return config;
}

int validate_config(traffic_config_t *config) {
    if (!config) return -1;

    // Validate business hours
    if (config->business.start_hour < 0 || config->business.start_hour > 23) return -1;
    if (config->business.end_hour < 0 || config->business.end_hour > 23) return -1;
    if (config->business.min_requests < 0) return -1;
    if (config->business.min_requests >= config->business.max_requests) return -1;
    if (config->business.max_requests > 500) return -1;

    // Validate evening hours
    if (config->evening.start_hour < 0 || config->evening.start_hour > 23) return -1;
    if (config->evening.end_hour < 0 || config->evening.end_hour > 23) return -1;
    if (config->evening.min_requests < 0) return -1;
    if (config->evening.min_requests >= config->evening.max_requests) return -1;
    if (config->evening.max_requests > 500) return -1;

    // Validate night hours
    if (config->night.start_hour < 0 || config->night.start_hour > 23) return -1;
    if (config->night.end_hour < 0 || config->night.end_hour > 23) return -1;
    if (config->night.min_requests < 0) return -1;
    if (config->night.min_requests >= config->night.max_requests) return -1;
    if (config->night.max_requests > 500) return -1;

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

    // Initialize with zeros
    memset(config, 0, sizeof(traffic_config_t));

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // Parse key=value
        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            int val = atoi(value);

            // Business hours
            if (strcmp(key, "business_start") == 0) {
                config->business.start_hour = val;
            } else if (strcmp(key, "business_end") == 0) {
                config->business.end_hour = val;
            } else if (strcmp(key, "business_min") == 0) {
                config->business.min_requests = val;
            } else if (strcmp(key, "business_max") == 0) {
                config->business.max_requests = val;
            }
            // Evening hours
            else if (strcmp(key, "evening_start") == 0) {
                config->evening.start_hour = val;
            } else if (strcmp(key, "evening_end") == 0) {
                config->evening.end_hour = val;
            } else if (strcmp(key, "evening_min") == 0) {
                config->evening.min_requests = val;
            } else if (strcmp(key, "evening_max") == 0) {
                config->evening.max_requests = val;
            }
            // Night hours
            else if (strcmp(key, "night_start") == 0) {
                config->night.start_hour = val;
            } else if (strcmp(key, "night_end") == 0) {
                config->night.end_hour = val;
            } else if (strcmp(key, "night_min") == 0) {
                config->night.min_requests = val;
            } else if (strcmp(key, "night_max") == 0) {
                config->night.max_requests = val;
            }
        }
    }

    fclose(fp);

    // Validate the loaded config
    if (validate_config(config) != 0) {
        printf("Invalid config values, using defaults\n");
        free_config(config);
        return get_default_config();
    }

    return config;
}
