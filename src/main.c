/** @file main.c @brief svgd-collect entry point */
#include "config.h"
#include "collect.h"
#include <signal.h>
#include <stdio.h>

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

    collect_run(&cfg);
    fprintf(stderr, "svgd-collect: shutting down\n");
    return 0;
}
