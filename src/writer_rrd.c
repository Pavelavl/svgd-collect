/** @file writer_rrd.c @brief write metrics to RRD via librrd (drop-in, direct path) */
#include "writer_rrd.h"
#include "types.h"
#include "path.h"
#include "rra.h"
#include <rrd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

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

/** Create all intermediate dirs in @p dir, ignoring EEXIST. */
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
            (void)mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }
    (void)mkdir(tmp, 0755);
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
        if (rrd_create_r(path, 5 /*step*/, 0 /*last_up*/, n, (const char **)argv) != 0) {
            fprintf(stderr, "rrd_create_r(%s): %s\n", path, rrd_get_error());
            rrd_clear_error();
            return -1;
        }
    }

    /* 5. Build "N:v1:v2..." and update. */
    char vbuf[256];
    size_t off = 0;
    vbuf[off++] = 'N';
    for (int i = 0; i < m->ds_count; i++) {
        int wlen = snprintf(vbuf + off, sizeof(vbuf) - off, ":%.6g", m->values[i]);
        if (wlen < 0 || (size_t)wlen >= sizeof(vbuf) - off) {
            return -1;   /* encoding error or truncation */
        }
        off += (size_t)wlen;
    }

    const char *uargv[1] = { vbuf };
    int rc = rrd_update_r(path, NULL, 1, uargv);
    if (rc != 0) {
        fprintf(stderr, "rrd_update_r(%s): %s\n", path, rrd_get_error());
        rrd_clear_error();
    }
    return rc;
}
