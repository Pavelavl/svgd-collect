/** @file collect.h @brief main collection loop */
#ifndef SVGD_COLLECT_COLLECT_H
#define SVGD_COLLECT_COLLECT_H
#include "config.h"
/** Run the collection loop until the global g_running flag is cleared (by a signal). */
void collect_run(collect_config_t *cfg);
#endif
