cmake_minimum_required(VERSION 3.28)
project(prescal)

include_directories(include)

set (
    SOURCES
    main.c
    src/engine.c
    src/dclient.c
    src/http.c
    src/configuration.c
    src/timer.c
    src/metrics.c
    src/epoll.c
    src/tsdb.c
    src/helper.c
    ds/linkedlist.c
)

add_executable(prescal ${SOURCES})