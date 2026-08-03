/** @file rra.h @brief build DS+RRA argv for rrd_create_r, matching collectd's RRATimespan logic */
#ifndef SVGD_COLLECT_RRA_H
#define SVGD_COLLECT_RRA_H
#include "types.h"
/** Fill argv[] with DS:... entries (one per DS of the type) followed by 3 RRA:AVERAGE:... entries
 *  (one per timespan). Each argv[i] points into an internal static buffer (NOT reentrant — single
 *  thread only, which is fine for svgd-collect's single-threaded loop).
 *  Returns the number of argv entries filled, or -1 if argv_max is too small. */
int rra_args(const type_def_t *td, char **argv, int argv_max);
#endif
