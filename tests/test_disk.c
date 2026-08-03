/** @file test_disk.c @brief disk reader: ops/octets/time per real disk (skip partitions) */
#include "minitest.h"
#include "metric.h"
#include "reader.h"

static metric_t captured[16];
static char inst_buf[16][64];   /* deep storage: plugin_instance may point at
                                 * reader-local storage valid only during emit */
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
    }
    cap_count++;
    return 0;
}
static const metric_t *find(const char *type, const char *inst) {
    int n = cap_count < 16 ? cap_count : 16;
    for (int i = 0; i < n; i++) {
        if (captured[i].type && strcmp(captured[i].type, type) == 0 &&
            captured[i].plugin_instance &&
            strcmp(captured[i].plugin_instance, inst) == 0) {
            return &captured[i];
        }
    }
    return NULL;
}

TEST(disk_emits_metrics_for_sda_and_dm0_skips_partitions) {
    cap_count = 0;
    ASSERT(disk_reader.read("tests/fixtures/proc_disk", cap_cb, NULL) == 0);
    /* sda + dm-0 (3 metrics each); the sda1 partition is skipped */
    ASSERT(cap_count == 6);
    for (int i = 0; i < 6; i++) {
        ASSERT_STR(captured[i].plugin, "disk");
        ASSERT(captured[i].plugin_instance != NULL);
        ASSERT(captured[i].ds_count == 2);
        /* whole-disk metrics carry no type_instance (no per-subpath split) */
        ASSERT(captured[i].type_instance == NULL);
    }
    /* fixture order is sda, sda1[skip], dm-0 -> first three are sda */
    for (int i = 0; i < 3; i++) {
        ASSERT_STR(captured[i].plugin_instance, "sda");
    }
    for (int i = 3; i < 6; i++) {
        ASSERT_STR(captured[i].plugin_instance, "dm-0");
    }
    const metric_t *ops_sda = find("disk_ops", "sda");
    const metric_t *oct_sda = find("disk_octets", "sda");
    const metric_t *tim_sda = find("disk_time", "sda");
    ASSERT(ops_sda != NULL);
    ASSERT(oct_sda != NULL);
    ASSERT(tim_sda != NULL);
    /* sda: reads_comp=100, writes_comp=200 */
    ASSERT(ops_sda->values[0] == 100.0);
    ASSERT(ops_sda->values[1] == 200.0);
    /* sda: sectors_read=2000 *512 =1024000; sectors_written=4000 *512 =2048000 */
    ASSERT(oct_sda->values[0] == 1024000.0);
    ASSERT(oct_sda->values[1] == 2048000.0);
    /* sda: ms_read=500, ms_write=600 */
    ASSERT(tim_sda->values[0] == 500.0);
    ASSERT(tim_sda->values[1] == 600.0);

    /* dm-0 was skipped before the regex was widened; now emits 3 metrics */
    const metric_t *ops_dm = find("disk_ops", "dm-0");
    const metric_t *oct_dm = find("disk_octets", "dm-0");
    const metric_t *tim_dm = find("disk_time", "dm-0");
    ASSERT(ops_dm != NULL);
    ASSERT(oct_dm != NULL);
    ASSERT(tim_dm != NULL);
    /* dm-0: reads_comp=30, writes_comp=60 */
    ASSERT(ops_dm->values[0] == 30.0);
    ASSERT(ops_dm->values[1] == 60.0);
    /* dm-0: sectors_read=600 *512 =307200; sectors_written=1200 *512 =614400 */
    ASSERT(oct_dm->values[0] == 307200.0);
    ASSERT(oct_dm->values[1] == 614400.0);
    /* dm-0: ms_read=150, ms_write=180 */
    ASSERT(tim_dm->values[0] == 150.0);
    ASSERT(tim_dm->values[1] == 180.0);
}

TEST_MAIN() RUN(disk_emits_metrics_for_sda_and_dm0_skips_partitions); TEST_RETURN()
