/** @file collect.c @brief interval loop: config -> readers -> writer */
#include "collect.h"
#include "writer_rrd.h"
#include "reader.h"
#include "metric.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

extern volatile sig_atomic_t g_running;   /* defined in main.c */

/** Name -> reader registry. The 9 implemented readers; the order is the
 *  enable-all order when config.readers[] is empty. Add new readers here. */
static const struct { const char *name; const reader_t *reader; } REGISTRY[] = {
    {"cpu",        &cpu_reader},
    {"load",       &load_reader},
    {"uptime",     &uptime_reader},
    {"memory",     &memory_reader},
    {"swap",       &swap_reader},
    {"interface",  &interface_reader},
    {"disk",       &disk_reader},
    {"df",         &df_reader},
    {"processes",  &processes_reader},
};
#define REGISTRY_N (sizeof(REGISTRY) / sizeof(REGISTRY[0]))

static int emit_cb(const metric_t *m, void *ud) {
    return writer_write((writer_t *)ud, m);
}

void collect_run(collect_config_t *cfg) {
    writer_t w;
    writer_init(&w, cfg->datadir, cfg->host, cfg->rrdcached);

    /* Build the enabled-reader list from config.readers[], or all registry
     * entries if readers[] is empty (meaning "all"). Unknown names in config
     * are warned about and skipped rather than aborting the run. */
    const reader_t *enabled[32];
    int n = 0;

    if (cfg->readers_count == 0) {
        for (size_t i = 0; i < REGISTRY_N && n < 32; i++)
            enabled[n++] = REGISTRY[i].reader;
    } else {
        for (int i = 0; i < cfg->readers_count; i++) {
            const char *want = cfg->readers[i];
            int found = 0;
            for (size_t j = 0; j < REGISTRY_N; j++) {
                if (strcmp(want, REGISTRY[j].name) == 0) {
                    if (n < 32) enabled[n++] = REGISTRY[j].reader;
                    found = 1;
                    break;
                }
            }
            if (!found)
                fprintf(stderr, "collect: unknown reader \"%s\"; skipping\n", want);
        }
    }

    while (g_running) {
        for (int i = 0; i < n; i++)
            enabled[i]->read("/proc", emit_cb, &w);
        /* interruptible sleep: wake each second to re-check g_running */
        for (int s = 0; s < cfg->interval && g_running; s++) sleep(1);
    }
}
