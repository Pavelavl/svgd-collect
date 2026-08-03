/** @file writer_rrd.c @brief write metrics to RRD via librrd (drop-in, direct path) */
#include "writer_rrd.h"
#include "types.h"
#include "path.h"
#include "rra.h"
#include <rrd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int writer_init(writer_t *w, const char *datadir, const char *host, const char *rrdcached)
{
    if (w == NULL || datadir == NULL || host == NULL) {
        return -1;
    }
    strncpy(w->datadir, datadir, sizeof(w->datadir) - 1);
    w->datadir[sizeof(w->datadir) - 1] = '\0';
    strncpy(w->host, host, sizeof(w->host) - 1);
    w->host[sizeof(w->host) - 1] = '\0';
    /* rrdcached is stored but UNUSED in Phase 1 (direct librrd path only). */
    if (rrdcached != NULL) {
        strncpy(w->rrdcached, rrdcached, sizeof(w->rrdcached) - 1);
        w->rrdcached[sizeof(w->rrdcached) - 1] = '\0';
    } else {
        w->rrdcached[0] = '\0';
    }
    return 0;
}

/** Create all intermediate dirs in @p dir, ignoring EEXIST. Other mkdir errors
 *  (EACCES, EROFS, ENAMETOOLONG, ...) are logged to stderr instead of being
 *  swallowed silently — a silent failure here would later surface as a confusing
 *  rrd_create/rrd_update error. */
static void mkdirp(const char *dir)
{
    char tmp[4096];
    size_t len = strlen(dir);
    if (len == 0 || len >= sizeof(tmp)) {
        return;
    }
    memcpy(tmp, dir, len + 1);
    /* Start at i=1 so a leading '/' is not treated as a separator. */
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "mkdirp(%s): %s\n", tmp, strerror(errno));
            }
            tmp[i] = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "mkdirp(%s): %s\n", tmp, strerror(errno));
    }
}

int fmt_values(char *out, size_t n, const type_def_t *td, const metric_t *m)
{
    if (out == NULL || m == NULL || td == NULL || n == 0) {
        return -1;
    }
    if (td->ds_count != m->ds_count) {
        return -1;
    }
    /* Build "N:v1:v2...". Per-DST format: DERIVE/COUNTER raw counters are integers
     * from /proc and can reach ~1.8e19 (UINT64_MAX-ish); %.0f keeps them in plain
     * decimal (no 'e'), whereas %.17g switches to scientific at >=1e17 and would be
     * rejected by rrd_update. GAUGE values use %.17g for round-trip safety. */
    size_t off = 0;
    out[off++] = 'N';
    for (int i = 0; i < m->ds_count; i++) {
        dst_t dst = (i < td->ds_count) ? td->ds[i].dst : DST_GAUGE;
        const char *fmt = (dst == DST_DERIVE || dst == DST_COUNTER) ? ":%.0f" : ":%.17g";
        int wlen = snprintf(out + off, n - off, fmt, m->values[i]);
        if (wlen < 0 || (size_t)wlen >= n - off) {
            return -1;   /* encoding error or truncation */
        }
        off += (size_t)wlen;
    }
    return 0;
}

int writer_write(writer_t *w, const metric_t *m)
{
    if (w == NULL || m == NULL) {
        return -1;
    }

    /* 1. Resolve the type; verify the DS count matches the values we carry. */
    const type_def_t *td = types_lookup(m->type);
    if (td == NULL || td->ds_count != m->ds_count) {
        fprintf(stderr, "writer_write: type '%s' ds_count mismatch\n",
                m->type ? m->type : "(null)");
        return -1;
    }

    /* 2. Build the collectd-layout RRD path. */
    char path[4096];
    if (metric_to_path(path, sizeof path, w->datadir, w->host, m) != 0) {
        return -1;
    }

    /* 3. mkdir -p the directory portion (everything up to the last '/'). */
    const char *slash = strrchr(path, '/');
    if (slash != NULL && slash != path) {
        size_t dlen = (size_t)(slash - path);
        char dirbuf[4096];
        if (dlen < sizeof(dirbuf)) {
            memcpy(dirbuf, path, dlen);
            dirbuf[dlen] = '\0';
            mkdirp(dirbuf);
        }
    }

    /* 4. Create the RRD lazily if it does not yet exist. */
    if (access(path, F_OK) != 0) {
        char *argv[64];
        int n = rra_args(td, argv, 64);
        if (n < 0) {
            return -1;
        }
        /* last_up = now-1 (collectd does last_up -= 1): a same-second create+update
         * otherwise gets rejected by librrd as "illegal attempt to update using an
         * time older than the last update" / older-than-step edge cases. */
        if (rrd_create_r(path, 5 /*step*/, time(NULL) - 1 /*last_up*/, n, (const char **)argv) != 0) {
            fprintf(stderr, "rrd_create_r(%s): %s\n", path, rrd_get_error());
            rrd_clear_error();
            return -1;
        }
    }

    /* 5. Build "N:v1:v2..." and update. */
    char vbuf[256];
    if (fmt_values(vbuf, sizeof vbuf, td, m) != 0) {
        return -1;
    }

    const char *uargv[1] = { vbuf };
    int rc = rrd_update_r(path, NULL, 1, uargv);
    if (rc != 0) {
        fprintf(stderr, "rrd_update_r(%s): %s\n", path, rrd_get_error());
        rrd_clear_error();
    }
    return rc;
}
