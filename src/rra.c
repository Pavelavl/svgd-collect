/** @file rra.c @brief build DS+RRA argv for rrd_create_r, matching collectd's RRATimespan logic */
#include "rra.h"
#include <stdio.h>

/* Constants — must match collectd's RRATimespan RRA generation (drop-in compatible). */
#define STEP 5                       /**< rrd step, seconds */
#define HEARTBEAT (STEP * 2)         /**< = 10s */
#define XFF "0.1"                    /**< xfiles factor */
#define RRAROWS 2400                 /**< target rows per RRA before pdp consolidation */

/** timespans (seconds): 1h, 24h, 7d — same as collectd's default RRATimespan list. */
static const long TIMESPANS[] = {3600L, 86400L, 604800L};
#define NTIMESPANS ((int)(sizeof(TIMESPANS) / sizeof(TIMESPANS[0])))

/* Internal backing store for the argv strings. Single-threaded, non-reentrant by design.
 * Generous size: up to 64 entries ~40 chars each. */
static char buf[64 * 40];

/** Map a dst_t to its RRD DST string. */
static const char *dst_str(dst_t d)
{
    switch (d) {
        case DST_GAUGE:  return "GAUGE";
        case DST_DERIVE: return "DERIVE";
        case DST_COUNTER:return "COUNTER";
        default:         return "GAUGE";
    }
}

int rra_args(const type_def_t *td, char **argv, int argv_max)
{
    if (td == NULL) {
        return -1;
    }

    int count = td->ds_count + NTIMESPANS;
    if (count > argv_max) {
        return -1;
    }

    char *p = buf;
    size_t remaining = sizeof(buf);
    int i = 0;

    /* DS entries: one per data source of the type. */
    for (int d = 0; d < td->ds_count; d++) {
        int len = snprintf(p, remaining, "DS:%s:%s:%d:U:U",
                           td->ds[d].name, dst_str(td->ds[d].dst), HEARTBEAT);
        argv[i++] = p;
        p += len + 1;            /* +1 for the NUL terminator */
        remaining -= (size_t)len + 1;
    }

    /* RRA entries: one per timespan, replicating collectd's RRATimespan consolidation. */
    for (int t = 0; t < NTIMESPANS; t++) {
        long span = TIMESPANS[t];
        long denom = (long)STEP * RRAROWS;          /* = 12000 */
        long pdp = (span + denom - 1) / denom;       /* ceil_div(span, denom) */
        long rows = span / ((long)STEP * pdp);       /* integer division */
        int len = snprintf(p, remaining, "RRA:AVERAGE:%s:%ld:%ld", XFF, pdp, rows);
        argv[i++] = p;
        p += len + 1;
        remaining -= (size_t)len + 1;
    }

    return count;
}
