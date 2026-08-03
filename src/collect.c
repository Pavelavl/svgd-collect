/** @file collect.c @brief interval loop: config -> readers -> writer */
#include "collect.h"
#include "writer_rrd.h"
#include "registry.h"
#include "metric.h"
#include <stdio.h>
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

    /* Resolve config.readers[] -> reader_t* list via the registry. The registry
     * owns the name->reader table; collect_build_enabled handles the empty
     * ("collect everything"), unknown-name, and duplicate cases. cfg->readers
     * is char[16][32] (not char**), so build a pointer array for the resolver. */
    const reader_t *enabled[32];
    const char *rptr[16];
    for (int i = 0; i < cfg->readers_count && i < 16; i++) {
        rptr[i] = cfg->readers[i];
    }
    int n = collect_build_enabled(rptr, cfg->readers_count, enabled, 32);

    while (g_running) {
        /* A reader returns -1 on a missing /proc file or parse error and logs
         * the specifics itself; the return value is intentionally ignored here
         * so a transient per-reader failure doesn't abort the whole loop, and
         * doesn't spam a per-interval message at the dispatcher level. */
        for (int i = 0; i < n; i++) {
            (void)enabled[i]->read("/proc", emit_cb, &w);
        }
        /* interruptible sleep: wake each second to re-check g_running */
        for (int s = 0; s < cfg->interval && g_running; s++) {
            sleep(1);
        }
    }

    /* On graceful shutdown, flush any rrdcached-buffered writes so they are
     * durable before exit. No-op when no daemon is configured (direct writes). */
    writer_shutdown(&w);
}
