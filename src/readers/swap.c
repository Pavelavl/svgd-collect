/** @file swap.c @brief Swap reader: used/free bytes from <proc_base>/meminfo */
#include <stdio.h>
#include <string.h>

#include "reader.h"

/**
 * @brief Read <proc_base>/meminfo and emit used/free swap, in bytes.
 *
 * Parses SwapTotal and SwapFree (kB integers). Keys are matched including the
 * trailing ':' so the prefix is anchored exactly ("SwapTotal:" vs
 * "SwapTotalCached:", etc.). Emits two "swap"/"swap" metrics:
 *   type_instance="used" = (SwapTotal - SwapFree) * 1024
 *   type_instance="free" = SwapFree * 1024
 * matching collectd's swap plugin. If SwapTotal is missing or non-positive
 * nothing is emitted (nothing to compute against).
 *
 * @param proc_base prefix path (real /proc, or a fixture dir for tests).
 * @param emit      callback that receives the emitted metrics.
 * @param ud        opaque user data passed through to emit (the writer).
 * @return 0 on success, -1 if <proc_base>/meminfo cannot be opened or
 *         SwapTotal is missing/non-positive.
 */
static int swap_read(const char *proc_base, metric_emit_fn emit, void *ud)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/meminfo", proc_base);

    FILE *f = fopen(path, "r");
    if (!f) {
        return -1;
    }

    long swap_total = -1, swap_free = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        long val;
        /* strncmp includes the ':' so the key is anchored at line start and
         * matched exactly (no prefix confusion). */
        if (strncmp(line, "SwapTotal:", 10) == 0 && sscanf(line, "%*s %ld", &val) == 1) {
            swap_total = val;
        } else if (strncmp(line, "SwapFree:", 9) == 0 && sscanf(line, "%*s %ld", &val) == 1) {
            swap_free = val;
        }
    }
    fclose(f);

    if (swap_total <= 0) {
        return -1;
    }

    metric_t m;
    m.plugin          = "swap";
    m.plugin_instance = NULL;
    m.type            = "swap";
    m.ds_count        = 1;

    m.type_instance = "used";
    m.values[0]     = (double)(swap_total - swap_free) * 1024.0;
    emit(&m, ud);

    m.type_instance = "free";
    m.values[0]     = (double)swap_free * 1024.0;
    emit(&m, ud);

    return 0;
}

const reader_t swap_reader = { "swap", swap_read };
