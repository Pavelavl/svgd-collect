/** @file prom.c @brief Prometheus text exposition: snapshot + formatter
 *
 * Design:
 *  - Snapshot store: two growable buffers (staging + published) of fixed-size
 *    metric copies. The collection thread fills staging (begin/add/publish); the
 *    HTTP thread reads published via prom_render(). The only shared mutation is
 *    the pointer swap in publish(), guarded by one mutex, and the memcpy in
 *    render(), guarded by the same mutex. Lock hold time is O(n) memcpy (no I/O),
 *    so the collection loop is never meaningfully blocked by a scrape.
 *  - Formatter: a rule table maps each collectd (plugin, type) to a Prometheus
 *    metric family name + the label names for plugin_instance/type_instance/DS.
 *    counter-vs-gauge is NOT hardcoded here — it is derived from the type's DS
 *    definitions (types.c): DERIVE/COUNTER -> counter, GAUGE -> gauge. This is
 *    the single source of truth for metric semantics.
 *
 * Timestamps are omitted (Prometheus assigns scrape time on ingest).
 */
#include "prom.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <math.h>

/** One deep-copied metric. Fixed-size so the snapshot is a flat array (no per-
 *  element malloc). NULL collectd-string fields collapse to "". */
typedef struct {
    char   plugin[48];
    char   plugin_instance[256];   /* mount instances can be long */
    char   type[48];
    char   type_instance[128];
    int    ds_count;
    double values[METRIC_MAX_DS];
} snap_metric_t;

/** Growable array of snap metrics. */
typedef struct {
    snap_metric_t *items;
    size_t count, cap;
} snap_buf_t;

/* ---- snapshot state (staging = collection thread; published = HTTP thread) -- */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static snap_buf_t *g_staged    = NULL;
static snap_buf_t *g_published = NULL;

/* ---- rule table ---------------------------------------------------------- */
/** Maps (plugin, type) to a Prometheus family. ds_label != NULL means each DS
 *  index becomes its own sample distinguished by ds_values[i]; NULL means the
 *  single DS is the value (no DS label). pi_label/ti_label name the labels for
 *  plugin_instance / type_instance, or NULL to omit that field. */
typedef struct {
    const char *plugin;
    const char *type;
    const char *family;        /* full family name, e.g. "svgd_network_bytes_total" */
    const char *pi_label;      /* label name for plugin_instance (NULL: omit) */
    const char *ti_label;      /* label name for type_instance (NULL: omit) */
    const char *ds_label;      /* label name distinguishing DS index (NULL: single-DS) */
    const char *ds_values[METRIC_MAX_DS];  /* label value per DS index (when ds_label) */
    const char *help;
} prom_rule_t;

/* The DS index order matches types.c (e.g. if_octets = {rx, tx}). */
static const prom_rule_t RULES[] = {
    /* cpu: plugin_instance is always "total" (aggregate) -> dropped as a constant;
     * type_instance ("active") carries the mode. */
    {"cpu", "percent", "svgd_cpu_percent", NULL, "mode", NULL, {NULL},
     "CPU busy percentage (delta between samples)."},
    {"memory", "percent", "svgd_memory_percent", NULL, "type", NULL, {NULL},
     "Memory utilization percentage by category."},
    {"swap", "swap", "svgd_swap_bytes", NULL, "state", NULL, {NULL},
     "Swap usage in bytes."},
    /* load: three DS (shortterm/midterm/longterm) -> interval label. */
    {"load", "load", "svgd_load", NULL, NULL, "interval", {"1m", "5m", "15m", NULL},
     "System load average (1/5/15-minute)."},
    {"uptime", "uptime", "svgd_uptime_seconds", NULL, NULL, NULL, {NULL},
     "System uptime in seconds."},
    /* interface: rx/tx -> direction label. Counters (DERIVE). */
    {"interface", "if_octets",  "svgd_network_bytes_total",   "device", NULL, "direction",
     {"receive", "transmit", NULL, NULL}, "Network bytes transferred."},
    {"interface", "if_packets", "svgd_network_packets_total", "device", NULL, "direction",
     {"receive", "transmit", NULL, NULL}, "Network packets transferred."},
    {"interface", "if_errors",  "svgd_network_errors_total",  "device", NULL, "direction",
     {"receive", "transmit", NULL, NULL}, "Network receive/transmit errors."},
    /* disk: read/write -> operation label. Counters (DERIVE). */
    {"disk", "disk_ops",    "svgd_disk_ops_total",       "device", NULL, "operation",
     {"read", "write", NULL, NULL}, "Disk I/O operations completed."},
    {"disk", "disk_octets", "svgd_disk_bytes_total",     "device", NULL, "operation",
     {"read", "write", NULL, NULL}, "Disk bytes read/written."},
    {"disk", "disk_time",   "svgd_disk_io_time_ms_total","device", NULL, "operation",
     {"read", "write", NULL, NULL}, "Cumulative disk I/O time in milliseconds."},
    /* df: mount instance -> fs label; used/free -> state label. Gauge (bytes). */
    {"df", "df_complex", "svgd_filesystem_bytes", "fs", "state", NULL, {NULL},
     "Filesystem used/free bytes."},
    /* processes: per-comm aggregation. ps_rss gauge (bytes); ps_cputime counter
     * (cumulative jiffies — raw clock ticks, see NOTES in README); ps_count gauge. */
    {"processes", "ps_rss",     "svgd_process_rss_bytes",         "process", NULL, NULL, {NULL},
     "Per-process-group resident set size (bytes)."},
    {"processes", "ps_cputime", "svgd_process_cpu_jiffies_total", "process", NULL, "mode",
     {"user", "system", NULL, NULL}, "Per-process-group cumulative CPU time (jiffies)."},
    {"processes", "ps_count",   "svgd_process_count",             "process", NULL, "kind",
     {"processes", "threads", NULL, NULL}, "Per-process-group instance and thread counts."},
    /* thermal: zone dir -> thermal_zone; zone type -> type. Gauge (degrees C). */
    {"thermal", "temperature", "svgd_thermal_celsius", "thermal_zone", "type", NULL, {NULL},
     "Thermal zone temperature (degrees Celsius)."},
    /* tcpconns: state name -> state label. Gauge (socket count). */
    {"tcpconns", "tcp_connections", "svgd_tcp_connections", NULL, "state", NULL, {NULL},
     "TCP socket count by connection state."},
};
#define RULES_N (sizeof(RULES) / sizeof(RULES[0]))

/* ============================ small helpers =============================== */

static const prom_rule_t *find_rule(const char *plugin, const char *type)
{
    for (size_t i = 0; i < RULES_N; i++) {
        if (strcmp(RULES[i].plugin, plugin) == 0 && strcmp(RULES[i].type, type) == 0) {
            return &RULES[i];
        }
    }
    return NULL;
}

/** Prometheus family type from the collectd type's first DS definition.
 *  All DS of a collectd type share the same DST (see types.c), so ds[0] suffices. */
static const char *prom_type_for(const char *type)
{
    const type_def_t *td = types_lookup(type);
    if (td == NULL || td->ds_count <= 0) {
        return "untyped";
    }
    return (td->ds[0].dst == DST_GAUGE) ? "gauge" : "counter";
}

static void copy_str(char *dst, size_t n, const char *src)
{
    if (n == 0) return;
    if (src == NULL) { dst[0] = '\0'; return; }
    size_t i = 0;
    for (; i + 1 < n && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static void copy_metric(snap_metric_t *dst, const metric_t *src)
{
    copy_str(dst->plugin,          sizeof dst->plugin,          src->plugin);
    copy_str(dst->plugin_instance, sizeof dst->plugin_instance, src->plugin_instance);
    copy_str(dst->type,            sizeof dst->type,            src->type);
    copy_str(dst->type_instance,   sizeof dst->type_instance,   src->type_instance);
    dst->ds_count = src->ds_count;
    if (dst->ds_count > METRIC_MAX_DS) {
        dst->ds_count = METRIC_MAX_DS;
    }
    for (int i = 0; i < dst->ds_count; i++) {
        dst->values[i] = src->values[i];
    }
}

/** Format a value cleanly: integer-valued magnitudes (counters, byte gauges) get
 *  %.0f (no decimals, no scientific notation); fractional gauges get %.10g. The
 *  integer check is bounded to 1e15 so we stay well inside double's exact-integer
 *  range (2^53 ~= 9e15). %.10g keeps realistic gauges decimal up to ~1e10
 *  (e.g. uptime in seconds covers ~317 years) without going scientific, while
 *  giving sub-precision-unit resolution on small fractional values. */
static void fmt_val(char *buf, size_t n, double v)
{
    if (!isfinite(v)) {
        snprintf(buf, n, "0");
        return;
    }
    if (v >= -1e15 && v <= 1e15 && v == floor(v)) {
        snprintf(buf, n, "%.0f", v);
    } else {
        snprintf(buf, n, "%.10g", v);
    }
}

/* ============================ dynamic string ============================== */

typedef struct {
    char  *buf;
    size_t len, cap;
} dstring_t;

static void ds_reserve(dstring_t *d, size_t extra)
{
    if (d->len + extra + 1 <= d->cap) return;
    size_t nc = d->cap ? d->cap : 512;
    while (nc < d->len + extra + 1) nc *= 2;
    char *nb = realloc(d->buf, nc);
    if (nb == NULL) return;        /* keep old cap; further appends become no-ops */
    d->buf = nb;
    d->cap = nc;
}

static void ds_append(dstring_t *d, const char *s, size_t n)
{
    ds_reserve(d, n);
    if (d->len + n + 1 > d->cap) return;
    memcpy(d->buf + d->len, s, n);
    d->len += n;
    d->buf[d->len] = '\0';
}

static void ds_appendf(dstring_t *d, const char *fmt, ...)
{
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) { va_end(ap2); return; }
    ds_reserve(d, (size_t)need);
    if (d->len + (size_t)need + 1 > d->cap) { va_end(ap2); return; }
    vsnprintf(d->buf + d->len, d->cap - d->len, fmt, ap2);
    va_end(ap2);
    d->len += (size_t)need;
}

/** Append a label value with Prometheus escaping: \ " and newline. */
static void append_escaped(dstring_t *d, const char *s)
{
    for (const char *p = s; *p; p++) {
        if (*p == '\\' || *p == '"' || *p == '\n') {
            char esc[2] = { '\\', (*p == '\n') ? 'n' : *p };
            ds_append(d, esc, 2);
        } else {
            ds_append(d, p, 1);
        }
    }
}

static void ds_append_label(dstring_t *d, const char *key, const char *val)
{
    if (d->len > 0) {              /* comma separator between labels */
        ds_append(d, ",", 1);
    }
    ds_append(d, key, strlen(key));
    ds_append(d, "=\"", 2);
    append_escaped(d, val);
    ds_append(d, "\"", 1);
}

/* ============================ rendering =================================== */

/** (rule, source index) pair, sorted by family so all samples of a family are
 *  contiguous and HELP/TYPE is emitted once before them. */
typedef struct {
    const prom_rule_t *r;
    size_t             i;
} entry_t;

static int cmp_family(const void *a, const void *b)
{
    const entry_t *x = a, *y = b;
    return strcmp(x->r->family, y->r->family);
}

static void render_to_dstring(dstring_t *d, const snap_metric_t *ms, size_t n)
{
    if (n == 0) {
        /* ensure a valid empty string */
        ds_reserve(d, 0);
        if (d->buf == NULL) { d->buf = realloc(NULL, 1); if (d->buf) { d->buf[0] = '\0'; } }
        return;
    }

    entry_t *e = malloc(n * sizeof(entry_t));
    if (e == NULL) return;
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        const prom_rule_t *r = find_rule(ms[i].plugin, ms[i].type);
        if (r == NULL) {
            continue;   /* unknown (plugin,type): skip, keep output clean */
        }
        e[m].r = r;
        e[m].i = i;
        m++;
    }
    qsort(e, m, sizeof(entry_t), cmp_family);

    const char *cur_family = NULL;
    for (size_t k = 0; k < m; k++) {
        const prom_rule_t *r = e[k].r;
        const snap_metric_t *sm = &ms[e[k].i];

        /* HELP/TYPE once per family (family strings are unique per rule). */
        if (cur_family == NULL || strcmp(cur_family, r->family) != 0) {
            ds_appendf(d, "# HELP %s %s\n", r->family, r->help);
            ds_appendf(d, "# TYPE %s %s\n", r->family, prom_type_for(sm->type));
            cur_family = r->family;
        }

        /* Emit one sample per DS (ds_count==1 -> one line, no DS label). */
        for (int di = 0; di < sm->ds_count; di++) {
            dstring_t labels = {0};
            if (r->pi_label != NULL && sm->plugin_instance[0] != '\0') {
                ds_append_label(&labels, r->pi_label, sm->plugin_instance);
            }
            if (r->ti_label != NULL && sm->type_instance[0] != '\0') {
                ds_append_label(&labels, r->ti_label, sm->type_instance);
            }
            if (r->ds_label != NULL && di < METRIC_MAX_DS && r->ds_values[di] != NULL) {
                ds_append_label(&labels, r->ds_label, r->ds_values[di]);
            }

            char valbuf[64];
            fmt_val(valbuf, sizeof valbuf, sm->values[di]);

            if (labels.len > 0) {
                ds_appendf(d, "%s{%s} %s\n", r->family, labels.buf, valbuf);
            } else {
                ds_appendf(d, "%s %s\n", r->family, valbuf);
            }
            free(labels.buf);
        }
    }
    free(e);
}

/* ============================ snapshot API ================================ */

static snap_buf_t *sb_new(void)
{
    return calloc(1, sizeof(snap_buf_t));
}

static void sb_reserve(snap_buf_t *sb, size_t need)
{
    if (sb->cap >= need) return;
    size_t nc = sb->cap ? sb->cap : 32;
    while (nc < need) nc *= 2;
    snap_metric_t *ni = realloc(sb->items, nc * sizeof(snap_metric_t));
    if (ni == NULL) return;
    sb->items = ni;
    sb->cap = nc;
}

void prom_snapshot_begin(void)
{
    if (g_staged == NULL) {
        g_staged = sb_new();
    }
    if (g_staged != NULL) {
        g_staged->count = 0;
    }
}

void prom_snapshot_add(const metric_t *m)
{
    if (g_staged == NULL) {
        g_staged = sb_new();
    }
    if (g_staged == NULL) {
        return;
    }
    if (g_staged->count >= g_staged->cap) {
        sb_reserve(g_staged, g_staged->count + 1);
    }
    if (g_staged->count < g_staged->cap) {
        copy_metric(&g_staged->items[g_staged->count], m);
        g_staged->count++;
    }
}

void prom_snapshot_publish(void)
{
    pthread_mutex_lock(&g_lock);
    snap_buf_t *tmp = g_published;
    g_published = g_staged;
    g_staged = tmp;             /* reuse old published buffer next cycle */
    pthread_mutex_unlock(&g_lock);
}

char *prom_render(void)
{
    pthread_mutex_lock(&g_lock);
    snap_buf_t *p = g_published;
    char *out;
    if (p == NULL || p->count == 0) {
        pthread_mutex_unlock(&g_lock);
        out = malloc(1);
        if (out != NULL) out[0] = '\0';
        return out;
    }
    snap_metric_t *copy = malloc(p->count * sizeof(snap_metric_t));
    if (copy == NULL) {
        pthread_mutex_unlock(&g_lock);
        out = malloc(1);
        if (out != NULL) out[0] = '\0';
        return out;
    }
    memcpy(copy, p->items, p->count * sizeof(snap_metric_t));
    size_t n = p->count;
    pthread_mutex_unlock(&g_lock);

    dstring_t d = {0};
    render_to_dstring(&d, copy, n);
    free(copy);
    if (d.buf == NULL) {
        out = malloc(1);
        if (out != NULL) out[0] = '\0';
        return out;
    }
    return d.buf;
}

char *prom_render_metrics(const metric_t *metrics, size_t n)
{
    snap_metric_t *snaps = NULL;
    if (n > 0) {
        snaps = malloc(n * sizeof(snap_metric_t));
        if (snaps == NULL) {
            char *e = malloc(1);
            if (e != NULL) e[0] = '\0';
            return e;
        }
        for (size_t i = 0; i < n; i++) {
            copy_metric(&snaps[i], &metrics[i]);
        }
    }
    dstring_t d = {0};
    render_to_dstring(&d, snaps, n);
    free(snaps);
    if (d.buf == NULL) {
        char *e = malloc(1);
        if (e != NULL) e[0] = '\0';
        return e;
    }
    return d.buf;
}
