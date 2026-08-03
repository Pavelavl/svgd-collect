/** @file collect.c @brief interval loop: config -> readers -> writer */
#include "collect.h"
#include "writer_rrd.h"
#include "reader.h"
#include "metric.h"
#include <unistd.h>
#include <signal.h>
#include <string.h>

extern volatile sig_atomic_t g_running;   /* defined in main.c */

static int emit_cb(const metric_t *m, void *ud) {
    return writer_write((writer_t *)ud, m);
}

void collect_run(collect_config_t *cfg) {
    writer_t w;
    writer_init(&w, cfg->datadir, cfg->host, cfg->rrdcached);

    /* Phase 1: only the cpu reader. Enable it if readers[] contains "cpu",
       or if readers[] is empty (meaning "all"). */
    const reader_t *readers[16];
    int n = 0;
    int want_cpu = (cfg->readers_count == 0);
    for (int i = 0; i < cfg->readers_count; i++)
        if (strcmp(cfg->readers[i], "cpu") == 0) want_cpu = 1;
    if (want_cpu && n < 16) readers[n++] = &cpu_reader;

    while (g_running) {
        for (int i = 0; i < n; i++)
            readers[i]->read("/proc", emit_cb, &w);
        /* interruptible sleep: wake each second to re-check g_running */
        for (int s = 0; s < cfg->interval && g_running; s++) sleep(1);
    }
}
