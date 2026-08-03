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

#endif
