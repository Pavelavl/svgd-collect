/** @file memory.c @brief Memory reader: used/cached/buffered % from <proc_base>/meminfo */
#include <stdio.h>
#include <string.h>

#include "reader.h"

/**
 * @brief Read <proc_base>/meminfo and emit used/cached/buffered memory %.
 *
 * Parses MemTotal, MemFree, Buffers and Cached (kB integers). Each key is
 * matched at line start including the ':' so prefixes are exact: "Cached:"
 * does not match "SwapCached:" (which starts with 'S'). Computes, all in kB:
 *   used = MemTotal - MemFree - Buffers - Cached
 * and emits three "memory"/"percent" metrics (type_instance used, cached,
 * buffered), each value = 100.0 * x / MemTotal — matching collectd's memory
 * plugin.
 *
 * @param proc_base prefix path (real /proc, or a fixture dir for tests).
 * @param emit      callback that receives the emitted metrics.
 * @param ud        opaque user data passed through to emit (the writer).
 * @return 0 on success, -1 if <proc_base>/meminfo cannot be opened, or
 *         MemTotal is missing/non-positive (would divide by zero).
 */
static int memory_read(const char *proc_base, metric_emit_fn emit, void *ud)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/meminfo", proc_base);

    FILE *f = fopen(path, "r");
    if (!f) {
        return -1;
    }

    long mem_total = -1, mem_free = 0, buffers = 0, cached = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        long val;
        /* strncmp includes the ':' so the key is anchored at line start and
         * is matched exactly (no prefix confusion, e.g. SwapCached/Cached). */
        if (strncmp(line, "MemTotal:", 9) == 0 && sscanf(line, "%*s %ld", &val) == 1) {
            mem_total = val;
        } else if (strncmp(line, "MemFree:", 8) == 0 && sscanf(line, "%*s %ld", &val) == 1) {
            mem_free = val;
        } else if (strncmp(line, "Buffers:", 8) == 0 && sscanf(line, "%*s %ld", &val) == 1) {
            buffers = val;
        } else if (strncmp(line, "Cached:", 7) == 0 && sscanf(line, "%*s %ld", &val) == 1) {
            cached = val;
        }
    }
    fclose(f);

    if (mem_total <= 0) {
        return -1;
    }

    long used = mem_total - mem_free - buffers - cached;

    metric_t m;
    m.plugin          = "memory";
    m.plugin_instance = NULL;
    m.type            = "percent";
    m.ds_count        = 1;

    m.type_instance = "used";
    m.values[0]     = 100.0 * (double)used / (double)mem_total;
    emit(&m, ud);

    m.type_instance = "cached";
    m.values[0]     = 100.0 * (double)cached / (double)mem_total;
    emit(&m, ud);

    m.type_instance = "buffered";
    m.values[0]     = 100.0 * (double)buffers / (double)mem_total;
    emit(&m, ud);

    return 0;
}

const reader_t memory_reader = { "memory", memory_read };
