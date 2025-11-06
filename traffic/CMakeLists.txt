cmake_minimum_required(VERSION 3.28)
project(traffic)

set(
    SOURCES
    main.c
    src/http.c
)

include_directories(include)
add_executable(traffic ${SOURCES})