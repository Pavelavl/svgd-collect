/** @file config.h @brief collect.json configuration */
#ifndef SVGD_COLLECT_CONFIG_H
#define SVGD_COLLECT_CONFIG_H
#include <stddef.h>
typedef struct {
    int  interval;                 /* seconds (default 5) */
    char datadir[4096];
    char host[128];
    char rrdcached[256];           /* parsed; unused in Phase 1 (direct librrd) */
    char readers[16][32];          /* enabled reader names */
    int  readers_count;
    char metrics_addr[128];        /* Prometheus /metrics bind addr ("host:port");
                                    * empty (default) -> no HTTP listener */
} collect_config_t;
/** Load config from a JSON file, applying defaults first. Returns 0 on success, -1 if file unreadable. */
int config_load(collect_config_t *c, const char *path);
#endif
