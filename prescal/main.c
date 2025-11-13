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


void linkedlist_test(void){
    struct linkedlist *ll = linkedlist_init();
    append_node(ll, "localhost:3000");
    append_node(ll, "localhost:3001");
    append_node(ll, "localhost:3002");
    print_ll(ll);
}

void start_engine(void) {
    struct prescal_engine *engine = engine_init("127.0.0.1", 80);
    start(engine);

    destroy_engine(engine);
}

void test_config(void) {
    struct prescal_config *config = config_init();
    read_config(config);
    print_config(config);
}

void docker_instance(void) {
    int docker_fd = dclient_connect();
    // dclient_new_instance(docker_fd);
    dclient_start_container(docker_fd);
    close(docker_fd);
}


int main() {
    start_engine();
    return 0;
}