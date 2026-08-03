/** @file prom.h @brief Prometheus exposition: metric snapshot + text-format rendering
 *
 * The collection loop tees every emitted metric into a staging buffer; at the end
 * of each interval it atomically publishes that buffer. The HTTP /metrics thread
 * renders the published snapshot on demand without blocking collection.
 *
 * Rendering maps each collectd (plugin, type) to a Prometheus metric family with
 * correct gauge/counter semantics (derived from the type's DS definitions in
 * types.c) and sensible names/labels. Unknown (plugin,type) pairs are skipped.
 */
#ifndef SVGD_COLLECT_PROM_H
#define SVGD_COLLECT_PROM_H
#include "metric.h"
#include <stddef.h>

/** Begin a new collection cycle: reset the staging buffer.
 *  Called by the collection thread at the top of each interval. */
void prom_snapshot_begin(void);

/** Deep-copy one metric into the current staging buffer (called from the emit
 *  callback, so the metric's strings need only be valid for the call). */
void prom_snapshot_add(const metric_t *m);

/** Publish the staged metrics atomically: swap staging <-> published under the
 *  snapshot lock. The next cycle reuses the old published buffer. */
void prom_snapshot_publish(void);

/** Render the published snapshot to a malloc'd Prometheus text-format string.
 *  Returns an empty string (never NULL) if no snapshot exists yet. Caller frees. */
char *prom_render(void);

/** Render an arbitrary metric array directly (no snapshot). Intended for unit
 *  tests. Returns a malloc'd string (never NULL). Caller frees. */
char *prom_render_metrics(const metric_t *metrics, size_t n);

#endif
