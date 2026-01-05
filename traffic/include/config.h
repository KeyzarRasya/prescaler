#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int base_min_requests;
    int base_max_requests;
    int high_traffic_min_requests;
    int high_traffic_max_requests;
    int normal_traffic_duration;
    int high_traffic_duration;
} traffic_config_t;

traffic_config_t* load_config(const char *config_path);
traffic_config_t* get_default_config(void);
int validate_config(traffic_config_t *config);
void free_config(traffic_config_t *config);

#endif