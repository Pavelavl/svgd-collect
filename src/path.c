/** @file path.c @brief collectd-layout RRD path builder */
#include "path.h"
#include <stdio.h>
#include <string.h>
static void append_seg(char *out, size_t n, size_t *off, const char *a, const char *b) {
    *off += (size_t)snprintf(out + *off, n - *off, "/%s%s%s", a, b ? "-" : "", b ? b : "");
}
int metric_to_path(char *out, size_t n, const char *base, const char *host, const metric_t *m) {
    if (!out || n == 0 || !base || !host || !m) return -1;
    size_t off = (size_t)snprintf(out, n, "%s/%s", base, host);
    if (off >= n) return -1;
    append_seg(out, n, &off, m->plugin, m->plugin_instance);
    /* type[-type_instance].rrd */
    char file[256];
    int fl = snprintf(file, sizeof file, "%s%s%s.rrd",
                      m->type, m->type_instance ? "-" : "", m->type_instance ? m->type_instance : "");
    if (fl < 0 || (size_t)fl >= sizeof file) return -1;
    off += (size_t)snprintf(out + off, n - off, "/%s", file);
    return (off >= n) ? -1 : 0;
}
