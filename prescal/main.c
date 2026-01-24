#define _POSIX_C_SOURCE 200809L
#include "include/http.h"
#include "include/dclient.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/engine.h"
#include "include/configuration.h"
#include "include/ds/linkedlist.h"
#include <time.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <path_to_config.yml>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *config_path = argv[1];

    // Initialize configuration from config.yml
    struct prescal_config *config = config_init();
    if (!config) {
        fprintf(stderr, "Failed to initialize configuration.\n");
        return EXIT_FAILURE;
    }
    read_config(config, config_path);


    // Initialize the engine with the loaded configuration
    struct prescal_engine *engine = engine_init(config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine.\n");
        // Note: config memory would be freed here if engine_init fails
        // but for now, we assume it won't fail if config is valid.
        return EXIT_FAILURE;
    }

    // Start the engine's main loop
    start(engine);

    // Cleanup is handled by destroy_engine
    destroy_engine(engine);

    return 0;
}
