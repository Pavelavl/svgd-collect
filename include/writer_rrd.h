/** @file writer_rrd.h @brief write metrics to RRD via librrd (drop-in, direct path) */
#ifndef SVGD_COLLECT_WRITER_RRD_H
#define SVGD_COLLECT_WRITER_RRD_H
#include "metric.h"
typedef struct {
    char datadir[4096];
    char host[128];
    char rrdcached[256];   /* parsed but UNUSED in Phase 1 (direct librrd only) */
} writer_t;
int writer_init(writer_t *w, const char *datadir, const char *host, const char *rrdcached);
/** Resolve type, build path, create RRD if missing, update. Returns 0 on success, -1 on error. */
int writer_write(writer_t *w, const metric_t *m);
#endif
