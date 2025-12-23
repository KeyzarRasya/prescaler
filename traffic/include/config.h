#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int start_hour;
    int end_hour;
    int min_requests;
    int max_requests;
} time_period_t;

typedef struct {
    time_period_t business;
    time_period_t evening;
    time_period_t night;
} traffic_config_t;

traffic_config_t* load_config(const char *config_path);
traffic_config_t* get_default_config(void);
int validate_config(traffic_config_t *config);
void free_config(traffic_config_t *config);

#endif
