/** @file main.c @brief svgd-collect entry point */
#include "config.h"
#include "collect.h"
#include "http.h"
#include <signal.h>
#include <stdio.h>
#include <pthread.h>

volatile sig_atomic_t g_running = 1;

static void on_signal(int sig) { (void)sig; g_running = 0; }

int main(int argc, char **argv) {
    const char *cfg_path = (argc > 1) ? argv[1] : "collect.json";

    struct sigaction sa;
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    collect_config_t cfg;
    if (config_load(&cfg, cfg_path) != 0)
        fprintf(stderr, "svgd-collect: cannot open %s; continuing with defaults\n", cfg_path);
    fprintf(stderr, "svgd-collect: interval=%ds datadir=%s host=%s\n",
            cfg.interval, cfg.datadir, cfg.host);

    /* Optional Prometheus /metrics endpoint. Strictly opt-in: an absent
     * metrics_addr starts no listener and behavior is unchanged. A bind failure
     * is logged but never aborts collection (the collector still writes RRDs). */
    pthread_t metrics_tid;
    int metrics_started = 0;
    if (cfg.metrics_addr[0] != '\0') {
        if (http_metrics_start(cfg.metrics_addr, &metrics_tid) == 0) {
            metrics_started = 1;
            fprintf(stderr, "svgd-collect: metrics endpoint on http://%s/metrics\n",
                    cfg.metrics_addr);
        } else {
            fprintf(stderr, "svgd-collect: metrics endpoint disabled (bind failed)\n");
        }
    }

    collect_run(&cfg);

    if (metrics_started) {
        http_metrics_join(metrics_tid);
    }
    fprintf(stderr, "svgd-collect: shutting down\n");
    return 0;
}
