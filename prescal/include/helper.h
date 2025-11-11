
#ifndef HELPER
#define HELPER

#include <stddef.h>

int read_env(const char *to_seach, char *dest, size_t dest_size);
int split_equal(const char *input, char **left, char **right);

#endif