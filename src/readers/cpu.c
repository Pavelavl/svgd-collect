/** @file cpu.c @brief CPU utilization reader: busy% delta from <proc_base>/stat */
#include <stdio.h>
#include <string.h>

#include "reader.h"

/** Previous-sample state (-1 = no previous sample yet). */
static long prev_busy = -1;
static long prev_total = -1;

/**
 * @brief Read the aggregate CPU line from <proc_base>/stat and emit busy%.
 *
 * Parses the "cpu " aggregate line (the trailing space distinguishes it from
 * per-core lines "cpu0", "cpu1", ...). CPU busy% is a delta between samples,
 * so the first call only primes the previous-sample state and emits nothing.
 *
 * @param proc_base prefix path (real /proc, or a fixture dir for tests).
 * @param emit      callback that receives the emitted metric.
 * @param ud        opaque user data passed through to emit (the writer).
 * @return 0 on success, -1 if <proc_base>/stat cannot be opened.
 */
static int cpu_read(const char *proc_base, metric_emit_fn emit, void *ud)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/stat", proc_base);

    FILE *f = fopen(path, "r");
    if (!f) {
        return -1;
    }

    char line[512];
    long user = 0, nice = 0, system = 0, idle = 0, iowait = 0;
    long irq = 0, softirq = 0, steal = 0, guest = 0, gnice = 0;
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        /* aggregate line starts with "cpu " (trailing space): skip per-core cpuN */
        if (strncmp(line, "cpu ", 4) == 0) {
            int n = sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
                           &user, &nice, &system, &idle, &iowait,
                           &irq, &softirq, &steal, &guest, &gnice);
            if (n >= 4) {
                found = 1;
            }
            break;
        }
    }
    fclose(f);

    if (!found) {
        return -1;
    }

    /* total = sum of all parsed fields; busy = total - idle - iowait */
    long total = user + nice + system + idle + iowait + irq + softirq + steal + guest + gnice;
    long busy  = total - idle - iowait;

    /* emit only once we have a previous sample and time has advanced */
    if (prev_busy >= 0 && (total - prev_total) > 0) {
        double pct = 100.0 * (double)(busy - prev_busy) / (double)(total - prev_total);
        metric_t m;
        m.plugin         = "cpu";
        /* plugin_instance="total" matches collectd's cpu aggregation: svgd reads
         * cpu-total/percent-active.rrd, so the path MUST be cpu-total/, not cpu/. */
        m.plugin_instance = "total";
        m.type           = "percent";
        m.type_instance  = "active";
        m.ds_count       = 1;
        m.values[0]      = pct;
        emit(&m, ud);
    }

    prev_busy  = busy;
    prev_total = total;
    return 0;
}

const reader_t cpu_reader = { "cpu", cpu_read };
