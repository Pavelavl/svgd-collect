/** @file http.h @brief minimal HTTP /metrics listener (Prometheus scrape target)
 *
 * Plain C sockets only (no libmicrohttpd/curl), matching svgd-collect's
 * zero-heavy-deps ethos. Serves GET /metrics in Prometheus text exposition
 * format and GET / (or /health) as a liveness probe; everything else is 404.
 * Single-threaded accept loop — sufficient for Prometheus scrape cadence
 * (typically every 15-60s).
 */
#ifndef SVGD_COLLECT_HTTP_H
#define SVGD_COLLECT_HTTP_H
#include <pthread.h>

/** Start the /metrics listener bound to @p addr ("host:port", "[ipv6]:port",
 *  ":port" for wildcard, or "" to skip). On success spawns a thread stored in
 *  @p tid and returns 0; on failure returns -1 (logged). The thread runs until
 *  the global g_running flag clears, then exits. */
int http_metrics_start(const char *addr, pthread_t *tid);

/** Block until the metrics thread exits. Call after g_running has been cleared. */
void http_metrics_join(pthread_t tid);

#endif
