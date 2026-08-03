/** @file registry.h @brief reader registry + enabled-list builder
 *
 * The registry maps reader names ("cpu", "thermal", ...) to their reader_t
 * pointers and is the single place new readers are registered. It is exposed
 * (rather than file-local in collect.c) so the unit tests can exercise lookup,
 * the empty-config ("collect everything") path, and duplicate/unknown handling
 * without driving the full interval loop.
 */
#ifndef SVGD_COLLECT_REGISTRY_H
#define SVGD_COLLECT_REGISTRY_H
#include "reader.h"

/** Look up a reader by name. Returns NULL if unknown or NULL. */
const reader_t *registry_find(const char *name);

/** Number of registered readers. */
int registry_count(void);

/** Build the enabled-reader list from config.readers[].
 *
 *  - If @p readers is NULL or @p count <= 0 ("collect everything"): copy all
 *    registered readers into @p out (up to @p max), in registry order.
 *  - Otherwise: resolve each requested name; unknown names are logged via
 *    log_err() and skipped; a reader named more than once runs at most once
 *    (duplicates are collapsed by pointer identity).
 *
 *  Returns the number of reader pointers written into @p out (never negative,
 *  never more than @p max). */
int collect_build_enabled(const char *const *readers, int count,
                          const reader_t *out[], int max);

#endif
