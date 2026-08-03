/** @file writer_rrd.h @brief write metrics to RRD via librrd (drop-in, direct path) */
#ifndef SVGD_COLLECT_WRITER_RRD_H
#define SVGD_COLLECT_WRITER_RRD_H
#include "metric.h"
#include "types.h"
#include <stddef.h>
typedef struct {
    char datadir[4096];
    char host[128];
    char rrdcached[256];   /* daemon address; empty -> direct librrd writes */
} writer_t;
int writer_init(writer_t *w, const char *datadir, const char *host, const char *rrdcached);
/** Resolve type, build path, create RRD if missing, update. Returns 0 on success, -1 on error. */
int writer_write(writer_t *w, const metric_t *m);
/** Flush + disconnect rrdcached if connected. No-op when no daemon is configured.
 *  Call once on graceful shutdown so buffered updates are durable. */
void writer_shutdown(writer_t *w);
/** Build an RRD update timestamp:value string "N:v1:v2..." into @p out. Precision is
 *  chosen per-DS from @p td: DERIVE/COUNTER use %.0f (raw /proc counters are integers;
 *  this stays decimal up to ~1.8e19 and avoids the scientific notation %.17g emits at
 *  >=1e17), GAUGE uses %.17g (round-trip-safe). Returns 0 on success, -1 on error/truncation. */
int fmt_values(char *out, size_t n, const type_def_t *td, const metric_t *m);
#endif
