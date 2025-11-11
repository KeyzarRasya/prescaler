#include <stdio.h>
#include "include/http.h"
#include "include/helper.h"

void test_read_env() {
    char token[256];
    read_env("TSDB_TOKEN", token, sizeof(token));

    printf("%s", token);
}

int main() {
    test_read_env();
    return 0;
}