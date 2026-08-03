/** @file rra.c @brief build DS+RRA argv for rrd_create_r, matching collectd's RRATimespan logic */
#include "rra.h"
#include <stdio.h>
#include <math.h>

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

    /* RRA entries: one AVERAGE RRA per timespan, replicating collectd's
     * rra_get() consolidation logic EXACTLY (src/utils/rrdcreate/rrdcreate.c).
     *
     * We emit AVERAGE only: svgd's reader (src/rrd/svg.c → rrd_fetch_data) always
     * asks librrd for the AVERAGE CF, so MIN/MAX RRAs would be invisible to it.
     * collectd normally also emits MIN/MAX, but omitting them wastes no work that
     * svgd consumes — this is the intentional drop-in simplification.
     *
     * Variable names mirror collectd: cdp_len == pdp_per_row, cdp_num == rows. */
    long cdp_len = 0;
    for (int t = 0; t < NTIMESPANS; t++) {
        long span = TIMESPANS[t];

        /* collectd: if (span / ss) < rrarows, grow span to ss*rrarows so even the
         * shortest timespan still yields rrarows rows at pdp_per_row=1. */
        if ((span / (long)STEP) < RRAROWS) {
            span = (long)STEP * RRAROWS;
        }

        /* First iteration keeps cdp_len=1; later iterations consolidate so the
         * row count stays near rrarows. */
        if (cdp_len == 0) {
            cdp_len = 1;
        } else {
            cdp_len = (long)floor((double)span / (double)(RRAROWS * (long)STEP));
        }
        long cdp_num = (long)ceil((double)span / (double)(cdp_len * (long)STEP));

        int len = snprintf(p, remaining, "RRA:AVERAGE:%s:%ld:%ld", XFF, cdp_len, cdp_num);
        if (len < 0 || (size_t)len >= remaining) {
            return -1;   /* backing buffer exhausted */
        }
        argv[i++] = p;
        p += len + 1;
        remaining -= (size_t)len + 1;
    }

    return count;
}
