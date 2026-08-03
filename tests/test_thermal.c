/** @file test_thermal.c @brief thermal reader: per-zone degrees C from sysfs */
#include "minitest.h"
#include "metric.h"
#include "reader.h"

static metric_t captured[16];
static char inst_buf[16][64];   /* deep storage: plugin_instance points into
                                 * reader-local storage valid only during emit */
static char ti_buf[16][64];     /* deep storage for type_instance */
static int cap_count = 0;
static int cap_cb(const metric_t *m, void *ud) {
    (void)ud;
    if (cap_count < 16) {
        captured[cap_count] = *m;
        if (m->plugin_instance) {
            strncpy(inst_buf[cap_count], m->plugin_instance, 63);
            inst_buf[cap_count][63] = '\0';
            captured[cap_count].plugin_instance = inst_buf[cap_count];
        }
        if (m->type_instance) {
            strncpy(ti_buf[cap_count], m->type_instance, 63);
            ti_buf[cap_count][63] = '\0';
            captured[cap_count].type_instance = ti_buf[cap_count];
        }
    }
    cap_count++;
    return 0;
}
static const metric_t *find_zone(const char *zone) {
    int n = cap_count < 16 ? cap_count : 16;
    for (int i = 0; i < n; i++) {
        if (captured[i].plugin_instance &&
            strcmp(captured[i].plugin_instance, zone) == 0) {
            return &captured[i];
        }
    }
    return NULL;
}

TEST(thermal_emits_per_zone_celsius) {
    cap_count = 0;
    ASSERT(thermal_reader.read("tests/fixtures/sys_thermal", cap_cb, NULL) == 0);
    ASSERT(cap_count == 2);

    const metric_t *z0 = find_zone("thermal_zone0");
    const metric_t *z1 = find_zone("thermal_zone1");
    ASSERT(z0 != NULL);
    ASSERT(z1 != NULL);

    for (int i = 0; i < 2; i++) {
        ASSERT_STR(captured[i].plugin, "thermal");
        ASSERT_STR(captured[i].type, "temperature");
        ASSERT(captured[i].ds_count == 1);
    }
    /* temp 45000 millideg -> 45.0 C; type file -> "x86_pkg_temp" (type_instance) */
    ASSERT(z0->values[0] == 45.0);
    ASSERT_STR(z0->type_instance, "x86_pkg_temp");
    /* temp 55000 millideg -> 55.0 C; type file -> "acpitz" */
    ASSERT(z1->values[0] == 55.0);
    ASSERT_STR(z1->type_instance, "acpitz");
}

TEST(thermal_missing_class_dir_returns_error) {
    cap_count = 0;
    /* nonexistent fixture base -> class/thermal missing -> -1 (logged, non-fatal) */
    ASSERT(thermal_reader.read("tests/fixtures/no_such_thermal", cap_cb, NULL) == -1);
    ASSERT(cap_count == 0);
}

TEST_MAIN() RUN(thermal_emits_per_zone_celsius); RUN(thermal_missing_class_dir_returns_error); TEST_RETURN()
