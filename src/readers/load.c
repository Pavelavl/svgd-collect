/** @file load.c @brief Load average reader: 1/5/15-min from <proc_base>/loadavg */
#include <stdio.h>

#include "reader.h"
#include "log.h"

/**
 * @brief Read <proc_base>/loadavg and emit the 1/5/15-minute load averages.
 *
 * The first line is "<1m> <5m> <15m> <runq/total> <lastpid>"; only the first
 * three doubles are parsed. Emits one "load" metric with three DS
 * (shortterm, midterm, longterm), matching collectd's load plugin.
 *
 * @param proc_base prefix path (real /proc, or a fixture dir for tests).
 * @param emit      callback that receives the emitted metric.
 * @param ud        opaque user data passed through to emit (the writer).
 * @return 0 on success, -1 if <proc_base>/loadavg cannot be opened/parsed.
 */
static int load_read(const char *proc_base, metric_emit_fn emit, void *ud)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/loadavg", proc_base);

    FILE *f = fopen(path, "r");
    if (!f) {
        log_errno("load", path);
        return -1;
    }

    double one = 0.0, five = 0.0, fifteen = 0.0;
    int n = fscanf(f, "%lf %lf %lf", &one, &five, &fifteen);
    fclose(f);

    if (n != 3) {
        log_err("load", "%s: expected 3 fields, got %d", path, n);
        return -1;
    }

    metric_t m;
    m.plugin          = "load";
    m.plugin_instance = NULL;
    m.type            = "load";
    m.type_instance   = NULL;
    m.ds_count        = 3;
    m.values[0]       = one;
    m.values[1]       = five;
    m.values[2]       = fifteen;
    emit(&m, ud);
    return 0;
}

const reader_t load_reader = { "load", load_read };
