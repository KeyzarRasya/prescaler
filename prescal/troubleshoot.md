# Troubleshooting Report: File Descriptor Leak in `prescal`

## 1. Summary

The `accept: Too many open files` error is caused by a critical file descriptor leak in the metrics collection logic. When a backend service (e.g., a Docker container on ports 3000-3002) becomes unreachable, the thread responsible for collecting metrics from that service enters a state where it creates a new socket every second but never closes it. This quickly exhausts the process's available file descriptors, preventing the main listener from accepting new incoming connections.

A secondary, unrelated memory leak was also identified and is documented below.

## 2. Root Cause Analysis: File Descriptor Leak

The file descriptor leak occurs in the `request_metrics` function within `src/metrics.c`.

### Problematic Code (`src/metrics.c`)

```c
int request_metrics(uint16_t port, char *out, size_t size) {
    int fd;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket\n");
        return -1;
    }

    if (connect_target(fd, port) < 0) {
        perror("Cannot connect to target instance\n");
        return -1; // LEAK: fd is not closed on connection failure
    }

    send(fd, METRICS_HTTP_REQ, strlen(METRICS_HTTP_REQ), 0);
    
    int n = recv(fd, out, size, 0);
    if (n < 0) {
        perror("recv\n");
        return -1; // LEAK: fd is not closed on receive failure
    }
    out[n] = '\0';
    close(fd);
    return 0;
}
```

### Explanation

1.  A new socket file descriptor (`fd`) is created by `socket()`.
2.  If `connect_target()` fails (e.g., the backend container is down or busy), the function prints an error and returns `-1`. **However, it fails to call `close(fd)` before returning.**
3.  Similarly, if `recv()` fails after a successful connection, the function also returns `-1` **without closing `fd`**.
4.  This behavior is triggered in a loop by the `request_per_second` function in `src/engine.c`, which attempts to collect metrics every second. If a backend is down, this thread will leak one file descriptor per second, every second.

## 3. Recommended Solution: File Descriptor Leak

To fix the leak, ensure `close(fd)` is called in all error-handling paths before the function returns.

### Corrected Code (`src/metrics.c`)

```c
int request_metrics(uint16_t port, char *out, size_t size) {
    int fd;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket\n");
        return -1;
    }

    if (connect_target(fd, port) < 0) {
        perror("Cannot connect to target instance\n");
        close(fd); // FIX: Close the file descriptor
        return -1;
    }

    send(fd, METRICS_HTTP_REQ, strlen(METRICS_HTTP_REQ), 0);
    
    int n = recv(fd, out, size, 0);
    if (n < 0) {
        perror("recv\n");
        close(fd); // FIX: Close the file descriptor
        return -1;
    }
    out[n] = '\0';
    close(fd);
    return 0;
}
```

## 4. Secondary Issue: Memory Leak

An unrelated memory leak was found in the `process_request` function in `src/engine.c`. The `http_req` object is being freed with `free()`, which does not release the memory allocated for its internal members.

### Problematic Code (`src/engine.c`)

```c
void process_request(int fd) {
    // ... (code omitted for brevity)

    struct http_req *hreq = http_req_init();

    if (handle_request(fd, hreq) != 0) {
        free(hreq); // LEAK: Does not free hreq->raw, hreq->method, etc.
        return;
    }
    
    // ... (code omitted for brevity)    
    
    free(hreq); // LEAK: Does not free hreq->raw, hreq->method, etc.
}
```

## 5. Recommended Solution: Memory Leak

Use the provided `http_req_clean()` function (which I assume exists based on `http_req_init` and good practice, but you should verify its existence and name in `http.h` or `http.c`) to correctly deallocate the entire `http_req` struct and its contents. If it doesn't exist, it should be created. Assuming `http_req_clean` is the correct function:

### Corrected Code (`src/engine.c`)

```c
#include "../include/http.h" // Ensure http.h is included

void process_request(int fd) {
    // ... (code omitted for brevity)

    struct http_req *hreq = http_req_init();

    if (handle_request(fd, hreq) != 0) {
        http_req_clean(hreq); // FIX: Use the correct cleanup function
        return;
    }
    
    // ... (code omitted for brevity)    
    
    http_req_clean(hreq); // FIX: Use the correct cleanup function
}
```
*(Note: The investigator tool suggested `http_req_clean`, which is a common pattern. Please verify the exact name of the cleanup function in your `http.c`/`http.h` files and use that.)*
