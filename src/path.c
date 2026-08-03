/** @file path.c @brief collectd-layout RRD path builder */
#include "path.h"
#include <stdio.h>
#include <string.h>

/* Append "/a[-b]" at out+*off. On truncation leave *off unchanged and return -1
 * (previously *off was bumped past `n`, making the next `n - off` underflow size_t
 * and write out of bounds). Returns 0 on success. */
static int append_seg(char *out, size_t n, size_t *off, const char *a, const char *b) {
    int wlen = snprintf(out + *off, n - *off, "/%s%s%s", a, b ? "-" : "", b ? b : "");
    if (wlen < 0 || (size_t)wlen >= n - *off) {
        return -1;   /* truncation: do NOT advance *off */
    }
    *off += (size_t)wlen;
    return 0;
}
int metric_to_path(char *out, size_t n, const char *base, const char *host, const metric_t *m) {
    if (!out || n == 0 || !base || !host || !m) return -1;
    size_t off = (size_t)snprintf(out, n, "%s/%s", base, host);
    if (off >= n) return -1;
    if (append_seg(out, n, &off, m->plugin, m->plugin_instance) != 0) return -1;
    /* type[-type_instance].rrd */
    char file[256];
    int fl = snprintf(file, sizeof file, "%s%s%s.rrd",
                      m->type, m->type_instance ? "-" : "", m->type_instance ? m->type_instance : "");
    if (fl < 0 || (size_t)fl >= sizeof file) return -1;
    int wlen = snprintf(out + off, n - off, "/%s", file);
    if (wlen < 0 || (size_t)wlen >= n - off) return -1;   /* truncation */
    off += (size_t)wlen;
    return 0;
}
