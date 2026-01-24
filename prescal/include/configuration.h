#ifndef CONFIGURATION
#define CONFIGURATION

#include <stdint.h>
#include "./ds/linkedlist.h"

#define PATH "/home/keyzarrasya/Documents/project/prescal/prescal/config.yml"

struct prescal_config{
    uint16_t port;
    char *entry;
    struct linkedlist *forwards;
};

struct prescal_config *config_init(void);
void read_config(struct prescal_config *config, const char *path);
void print_config(struct prescal_config *config);
void config_destroy(struct prescal_config *config);

#endif