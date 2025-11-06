#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/http.h"
#include <string.h>

struct http_req *http_req_init() {
    struct http_req *http = 
        malloc(sizeof(struct http_req));
    
    if (!http) {
        perror("Failed to allocate http_request");
        exit(EXIT_FAILURE);
        return NULL;        
    }

    http->method = NULL;
    http->path = NULL;
    http->http_v = NULL;
    http->raw = NULL;
    return http;
}

void convert_request(struct http_req *hreq, const char *request, size_t size) {
    char endpoint_info[255];
    hreq->raw = strdup(request);
    get_endpoint_info(request, endpoint_info, sizeof(endpoint_info));
    extract_req(hreq, endpoint_info);    
}

void extract_req(struct http_req *hreq, char *endpoint_info) {
    char *token = strtok(endpoint_info, " ");
    if (token) hreq->method = strdup(token);
    
    token = strtok(NULL, " ");
    if (token) hreq->path = strdup(token);

    token = strtok(NULL, " ");
    if (token) hreq->http_v = strdup(token);
}

void get_endpoint_info(const char *src, char *out, size_t size) {
    int i = 0;
    while (src[i] != '\n' && i < size - 1) {
        out[i] = src[i];
        i++;
    }
    out[i] = '\0';
}

void http_req_clean(struct http_req *hreq) {
    free(hreq->http_v);
    free(hreq->method);
    free(hreq->path);
    free(hreq->raw);
    free(hreq);
}

void req_to_string(struct http_req *hreq) {
    printf("Method %s\n Path %s\n Version %s\n", hreq->method, hreq->path, hreq->http_v);
}