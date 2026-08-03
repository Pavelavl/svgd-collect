/** @file metric.h @brief value model + emit callback for readers */
#ifndef SVGD_COLLECT_METRIC_H
#define SVGD_COLLECT_METRIC_H
#define METRIC_MAX_DS 4
typedef struct {
    const char *plugin;             /* "cpu", "memory", "interface", ... */
    const char *plugin_instance;    /* "total", "eth0", NULL if none */
    const char *type;               /* "percent", "if_octets", ... */
    const char *type_instance;      /* "active", "rx", NULL if none */
    int         ds_count;           /* number of values = number of DS */
    double      values[METRIC_MAX_DS];
} metric_t;
/** Reader → writer callback: emit one metric. Return 0 on success. */
typedef int (*metric_emit_fn)(const metric_t *m, void *ud);
#endif
