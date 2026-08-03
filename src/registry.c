/** @file registry.c @brief reader registry + enabled-list builder */
#include "registry.h"
#include "log.h"
#include <string.h>

/** Name -> reader registry. Add new readers here (and only here): the order is
 *  the default "collect everything" order when config.readers[] is empty. */
static const struct {
    const char    *name;
    const reader_t *reader;
} REGISTRY[] = {
    {"cpu",        &cpu_reader},
    {"load",       &load_reader},
    {"uptime",     &uptime_reader},
    {"memory",     &memory_reader},
    {"swap",       &swap_reader},
    {"interface",  &interface_reader},
    {"disk",       &disk_reader},
    {"df",         &df_reader},
    {"processes",  &processes_reader},
    {"thermal",    &thermal_reader},
    {"tcpconns",   &tcpconns_reader},
};
#define REGISTRY_N (sizeof(REGISTRY) / sizeof(REGISTRY[0]))

const reader_t *registry_find(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < REGISTRY_N; i++) {
        if (strcmp(name, REGISTRY[i].name) == 0) {
            return REGISTRY[i].reader;
        }
    }
    return NULL;
}

int registry_count(void)
{
    return (int)REGISTRY_N;
}

int collect_build_enabled(const char *const *readers, int count,
                          const reader_t *out[], int max)
{
    int n = 0;

    /* Empty config -> enable every registered reader, in registry order. */
    if (readers == NULL || count <= 0) {
        for (int i = 0; i < (int)REGISTRY_N && n < max; i++) {
            out[n++] = REGISTRY[i].reader;
        }
        return n;
    }

    for (int i = 0; i < count; i++) {
        const char *want = readers[i];
        const reader_t *r = registry_find(want);
        if (r == NULL) {
            log_err("registry", "unknown reader \"%s\"; skipping", want);
            continue;
        }
        /* Dedup by pointer identity: a reader listed twice (or aliased) runs
         * once per interval — double-sampling the same /proc file only wastes
         * work and would write the same value twice to the same RRD. */
        int dup = 0;
        for (int j = 0; j < n; j++) {
            if (out[j] == r) {
                dup = 1;
                break;
            }
        }
        if (dup) {
            continue;
        }
        if (n < max) {
            out[n++] = r;
        }
    }
    return n;
}
