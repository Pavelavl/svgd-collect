/** @file uptime.c @brief Uptime reader: seconds from <proc_base>/uptime */
#include <stdio.h>

#include "reader.h"

/**
 * @brief Read <proc_base>/uptime and emit system uptime in seconds.
 *
 * Parses the first double (uptime seconds); the trailing idle-seconds
 * value is ignored. Emits a single "uptime" metric, like collectd's uptime
 * plugin (svgd divides by 3600 for hours via its config transform).
 *
 * @param proc_base prefix path (real /proc, or a fixture dir for tests).
 * @param emit      callback that receives the emitted metric.
 * @param ud        opaque user data passed through to emit (the writer).
 * @return 0 on success, -1 if <proc_base>/uptime cannot be opened/parsed.
 */
static int uptime_read(const char *proc_base, metric_emit_fn emit, void *ud)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/uptime", proc_base);

    FILE *f = fopen(path, "r");
    if (!f) {
        return -1;
    }

    double up = 0.0;
    int n = fscanf(f, "%lf", &up);
    fclose(f);

    if (n != 1) {
        return -1;
    }

    metric_t m;
    m.plugin          = "uptime";
    m.plugin_instance = NULL;
    m.type            = "uptime";
    m.type_instance   = NULL;
    m.ds_count        = 1;
    m.values[0]       = up;
    emit(&m, ud);
    return 0;
}

const reader_t uptime_reader = { "uptime", uptime_read };
