/** @file writer_rrd.h @brief write metrics to RRD via librrd (drop-in, direct path) */
#ifndef SVGD_COLLECT_WRITER_RRD_H
#define SVGD_COLLECT_WRITER_RRD_H
#include "metric.h"
#include <stddef.h>
typedef struct {
    char datadir[4096];
    char host[128];
    char rrdcached[256];   /* parsed but UNUSED in Phase 1 (direct librrd only) */
} writer_t;
int writer_init(writer_t *w, const char *datadir, const char *host, const char *rrdcached);
/** Resolve type, build path, create RRD if missing, update. Returns 0 on success, -1 on error. */
int writer_write(writer_t *w, const metric_t *m);
/** Build an RRD update timestamp:value string "N:v1:v2..." into @p out using
 *  round-trip-safe precision (%.17g). Returns 0 on success, -1 on error/truncation. */
int fmt_values(char *out, size_t n, const metric_t *m);
#endif
