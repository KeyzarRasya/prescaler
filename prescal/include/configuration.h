#ifndef CONFIGURATION
#define CONFIGURATION

#include "./ds/linkedlist.h"
#include <pthread.h>
#include <stdint.h>

#define PATH "/home/keyzarrasya/Documents/project/prescal/prescal/config.yml"

struct prescal_config {
  uint16_t port;
  char *entry;
  struct linkedlist *forwards;
  pthread_mutex_t mutex;
};

struct prescal_config *config_init(void);
void read_config(struct prescal_config *config, const char *path);
void print_config(struct prescal_config *config);
void config_destroy(struct prescal_config *config);

#endif