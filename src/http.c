/** @file http.c @brief minimal HTTP /metrics listener (plain sockets)
 *
 * Lifecycle: http_metrics_start() resolves+binds the address, spawns a thread
 * running metrics_thread(), and returns. The thread polls the listen socket
 * with a 1s timeout so it notices g_running going to 0 within ~1s, then closes
 * the socket and exits. http_metrics_join() waits for that exit.
 *
 * Per-connection handling is deliberately tiny: read the request line, dispatch
 * on (method, path), write a fixed response, close. A 5s send/recv timeout
 * prevents a slow/stuck client from stalling the loop.
 */
#include "http.h"
#include "prom.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

extern volatile sig_atomic_t g_running;   /* defined in main.c */

/** Parse "host:port" / "[v6]:port" / ":port" into host (may be empty) and port.
 *  Returns 0 on success, -1 on malformed input. */
static int parse_addr(const char *addr, char *host, size_t hsz, char *port, size_t psz)
{
    const char *colon = strrchr(addr, ':');
    if (colon == NULL) {
        return -1;
    }
    /* port portion */
    const char *pp = colon + 1;
    if (*pp == '\0') {
        return -1;
    }
    for (const char *q = pp; *q; q++) {
        if (*q < '0' || *q > '9') {
            return -1;        /* port must be all digits */
        }
    }
    if (strlen(pp) >= psz) {
        return -1;
    }
    snprintf(port, psz, "%s", pp);

    /* host portion (before the last colon); strip IPv6 brackets. */
    size_t hlen = (size_t)(colon - addr);
    if (hlen == 0) {
        host[0] = '\0';       /* wildcard — bind to all interfaces */
        return 0;
    }
    if (hlen >= hsz) {
        return -1;
    }
    memcpy(host, addr, hlen);
    host[hlen] = '\0';
    if (host[0] == '[' && host[hlen - 1] == ']') {
        memmove(host, host + 1, hlen - 2);
        host[hlen - 2] = '\0';
    }
    return 0;
}

/** Create, bind, listen. Returns the fd or -1. */
static int make_listener(const char *addr)
{
    char host[256], port[16];
    if (parse_addr(addr, host, sizeof host, port, sizeof port) != 0) {
        log_err("metrics", "bad metrics_addr \"%s\" (expected \"host:port\")", addr);
        return -1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     /* bind wildcard when host is empty */

    struct addrinfo *res = NULL;
    int gai = getaddrinfo(host[0] ? host : NULL, port, &hints, &res);
    if (gai != 0) {
        log_err("metrics", "getaddrinfo(\"%s\",\"%s\"): %s",
                host, port, gai_strerror(gai));
        return -1;
    }

    int sfd = -1;
    for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd < 0) {
            continue;
        }
        int yes = 1;
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
#ifdef IPV6_V6ONLY
        /* Allow dual-stack on IPv6 sockets so "0.0.0.0"/"::" both work broadly. */
        int off = 0;
        setsockopt(sfd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof off);
#endif
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        log_errno("metrics", "bind");
        close(sfd);
        sfd = -1;
    }
    freeaddrinfo(res);
    if (sfd < 0) {
        log_err("metrics", "no usable address to bind on \"%s\"", addr);
        return -1;
    }
    if (listen(sfd, 16) != 0) {
        log_errno("metrics", "listen");
        close(sfd);
        return -1;
    }
    return sfd;
}

/** Write all n bytes, tolerating short writes and EINTR. */
static int send_all(int fd, const char *buf, size_t n)
{
    size_t off = 0;
    while (off < n) {
        ssize_t w = send(fd, buf + off, n - off, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (w == 0) return -1;
        off += (size_t)w;
    }
    return 0;
}

static void send_simple(int fd, int code, const char *reason, const char *body)
{
    char hdr[256];
    size_t blen = strlen(body);
    int hl = snprintf(hdr, sizeof hdr,
                      "HTTP/1.1 %d %s\r\n"
                      "Content-Type: text/plain; charset=utf-8\r\n"
                      "Content-Length: %zu\r\n"
                      "Connection: close\r\n"
                      "\r\n",
                      code, reason, blen);
    send_all(fd, hdr, (size_t)hl);
    send_all(fd, body, blen);
}

static void handle_conn(int cfd)
{
    /* Time out slow clients so the accept loop is never stalled for long. */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);

    char req[1024];
    ssize_t rn = recv(cfd, req, sizeof(req) - 1, 0);
    if (rn <= 0) {
        return;
    }
    req[rn] = '\0';

    /* Request line: "METHOD SP PATH SP HTTP/x.y". */
    char method[16], path[512];
    if (sscanf(req, "%15s %511s", method, path) < 2) {
        send_simple(cfd, 400, "Bad Request", "bad request\n");
        return;
    }

    if (strcmp(method, "GET") != 0) {
        send_simple(cfd, 405, "Method Not Allowed", "method not allowed\n");
        return;
    }

    if (strcmp(path, "/metrics") == 0) {
        char *body = prom_render();
        size_t blen = body ? strlen(body) : 0;
        char hdr[256];
        int hl = snprintf(hdr, sizeof hdr,
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n"
                          "\r\n", blen);
        send_all(cfd, hdr, (size_t)hl);
        if (blen > 0) {
            send_all(cfd, body, blen);
        }
        free(body);
        return;
    }

    if (strcmp(path, "/") == 0 || strcmp(path, "/health") == 0) {
        send_simple(cfd, 200, "OK", "svgd-collect\n");
        return;
    }

    send_simple(cfd, 404, "Not Found", "not found\n");
}

static void *metrics_thread(void *arg)
{
    int sfd = *(int *)arg;
    free(arg);

    while (g_running) {
        struct pollfd pfd = { .fd = sfd, .events = POLLIN, .revents = 0 };
        int rc = poll(&pfd, 1, 1000);   /* wake each second to re-check g_running */
        if (rc <= 0) {
            continue;                   /* timeout (rc==0) or transient error */
        }
        if ((pfd.revents & POLLIN) == 0) {
            continue;
        }
        int cfd = accept(sfd, NULL, NULL);
        if (cfd < 0) {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                log_errno("metrics", "accept");
            }
            continue;
        }
        handle_conn(cfd);
        close(cfd);
    }
    close(sfd);
    return NULL;
}

int http_metrics_start(const char *addr, pthread_t *tid)
{
    if (addr == NULL || addr[0] == '\0' || tid == NULL) {
        return -1;
    }
    int sfd = make_listener(addr);
    if (sfd < 0) {
        return -1;
    }
    int *fdp = malloc(sizeof(int));
    if (fdp == NULL) {
        close(sfd);
        return -1;
    }
    *fdp = sfd;
    if (pthread_create(tid, NULL, metrics_thread, fdp) != 0) {
        log_err("metrics", "pthread_create failed");
        free(fdp);
        close(sfd);
        return -1;
    }
    return 0;
}

void http_metrics_join(pthread_t tid)
{
    pthread_join(tid, NULL);
}
