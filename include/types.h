/** @file types.h @brief collectd-compatible type → DS definitions (drop-in) */
#ifndef SVGD_COLLECT_TYPES_H
#define SVGD_COLLECT_TYPES_H
typedef enum { DST_GAUGE, DST_DERIVE, DST_COUNTER } dst_t;
typedef struct { const char *name; dst_t dst; } ds_def_t;
typedef struct { const char *type; const ds_def_t *ds; int ds_count; } type_def_t;
/** Lookup a type definition by name. Returns NULL if unknown. */
const type_def_t *types_lookup(const char *type);
#endif
