#ifndef HTTP
#define HTTP

#include <stddef.h>

struct http_req {
    char *method;
    char *path;
    char *http_v;
    char *raw;
};

/* http req function definitions */
struct http_req *http_req_init();
void extract_req(struct http_req *hreq, char *endpoint_info);
void req_to_string(struct http_req *req);
void convert_request(struct http_req *req, const char *request, size_t size);
void get_endpoint_info(const char *src, char *out, size_t size);
void http_req_clean(struct http_req *hreq);

#endif