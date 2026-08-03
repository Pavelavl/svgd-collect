/** @file types.c @brief collectd-compatible type → DS definitions (drop-in) */
#include "types.h"
#include <stddef.h>
#include <string.h>

/* Per-type DS definitions. Names/DST must match collectd's types.db. */
static const ds_def_t ds_percent[]        = { {"value", DST_GAUGE} };
static const ds_def_t ds_if_octets[]      = { {"rx", DST_DERIVE}, {"tx", DST_DERIVE} };
static const ds_def_t ds_if_packets[]     = { {"rx", DST_DERIVE}, {"tx", DST_DERIVE} };
static const ds_def_t ds_if_errors[]      = { {"rx", DST_DERIVE}, {"tx", DST_DERIVE} };
static const ds_def_t ds_disk_ops[]       = { {"reads", DST_DERIVE}, {"writes", DST_DERIVE} };
static const ds_def_t ds_disk_octets[]    = { {"read", DST_DERIVE}, {"write", DST_DERIVE} };
static const ds_def_t ds_disk_time[]      = { {"read", DST_DERIVE}, {"write", DST_DERIVE} };
static const ds_def_t ds_ps_rss[]         = { {"value", DST_GAUGE} };
static const ds_def_t ds_ps_cputime[]     = { {"value", DST_DERIVE} };
static const ds_def_t ds_ps_count[]       = { {"processes", DST_GAUGE} };
static const ds_def_t ds_df_complex[]     = { {"value", DST_GAUGE} };
static const ds_def_t ds_load[]           = { {"shortterm", DST_GAUGE},
                                              {"midterm", DST_GAUGE},
                                              {"longterm", DST_GAUGE} };
static const ds_def_t ds_uptime[]         = { {"value", DST_GAUGE} };
static const ds_def_t ds_swap[]           = { {"used", DST_GAUGE}, {"free", DST_GAUGE} };
static const ds_def_t ds_temperature[]    = { {"value", DST_GAUGE} };
static const ds_def_t ds_tcp_connections[] = { {"value", DST_GAUGE} };

static const type_def_t TABLE[] = {
    {"percent",         ds_percent,         1},
    {"if_octets",       ds_if_octets,       2},
    {"if_packets",      ds_if_packets,      2},
    {"if_errors",       ds_if_errors,       2},
    {"disk_ops",        ds_disk_ops,        2},
    {"disk_octets",     ds_disk_octets,     2},
    {"disk_time",       ds_disk_time,       2},
    {"ps_rss",          ds_ps_rss,          1},
    {"ps_cputime",      ds_ps_cputime,      1},
    {"ps_count",        ds_ps_count,        1},
    {"df_complex",      ds_df_complex,      1},
    {"load",            ds_load,            3},
    {"uptime",          ds_uptime,          1},
    {"swap",            ds_swap,            2},
    {"temperature",     ds_temperature,     1},
    {"tcp_connections", ds_tcp_connections, 1},
};

#define TABLE_SIZE (sizeof(TABLE) / sizeof(TABLE[0]))

const type_def_t *types_lookup(const char *type)
{
    if (type == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        if (strcmp(TABLE[i].type, type) == 0) {
            return &TABLE[i];
        }
    }
    return NULL;
}
