/** @file path.h @brief build a collectd-layout RRD file path from a metric */
#ifndef SVGD_COLLECT_PATH_H
#define SVGD_COLLECT_PATH_H
#include <stddef.h>
#include "metric.h"
/** Build "<base>/<host>/<plugin>[-<plugin_instance>]/<type>[-<type_instance>].rrd".
 *  Instances that are NULL are omitted (no -suffix). Returns 0 on success, -1 on error/truncation. */
int metric_to_path(char *out, size_t n, const char *base, const char *host, const metric_t *m);
#endif
