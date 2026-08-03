/** @file reader.h @brief reader interface: a named function that emits metrics */
#ifndef SVGD_COLLECT_READER_H
#define SVGD_COLLECT_READER_H
#include "metric.h"

/**
 * @brief A reader is a named function that reads one sample from the system
 *        and emits metric(s) via the provided callback.
 */
typedef struct {
    const char *name;
    /** Read one sample from <proc_base>/... and emit metric(s) via the callback.
     *  Returns 0 on success, -1 on error. ud is passed through to emit (the writer). */
    int (*read)(const char *proc_base, metric_emit_fn emit, void *ud);
} reader_t;

/** CPU utilization reader: parses <proc_base>/stat, emits percent/active. */
extern const reader_t cpu_reader;

/** Load average reader: parses <proc_base>/loadavg, emits load (1/5/15-min). */
extern const reader_t load_reader;

/** Uptime reader: parses <proc_base>/uptime, emits uptime (seconds). */
extern const reader_t uptime_reader;

/** Memory reader: parses <proc_base>/meminfo, emits percent used/cached/buffered. */
extern const reader_t memory_reader;

/** Swap reader: parses <proc_base>/meminfo, emits swap used/free (bytes). */
extern const reader_t swap_reader;

/** Interface reader: parses <proc_base>/net/dev, emits rx/tx octets/packets/errors. */
extern const reader_t interface_reader;

/** Disk reader: parses <proc_base>/diskstats, emits ops/octets/time per real disk. */
extern const reader_t disk_reader;

/** DF reader: parses <proc_base>/mounts, emits per-mount df_complex used/free. */
extern const reader_t df_reader;

/** Processes reader: aggregates <proc_base>/[pid] by comm into ps_rss/cputime/count. */
extern const reader_t processes_reader;

#endif
