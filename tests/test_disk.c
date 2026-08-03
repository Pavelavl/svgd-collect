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
static const metric_t *find(const char *type) {
    int n = cap_count < 16 ? cap_count : 16;
    for (int i = 0; i < n; i++) {
        if (captured[i].type && strcmp(captured[i].type, type) == 0) {
            return &captured[i];
        }
    }
    return NULL;
}

TEST(disk_emits_ops_octets_time_for_sda_only) {
    cap_count = 0;
    ASSERT(disk_reader.read("tests/fixtures/proc_disk", cap_cb, NULL) == 0);
    /* only sda — sda1 partition is skipped */
    ASSERT(cap_count == 3);
    for (int i = 0; i < 3; i++) {
        ASSERT_STR(captured[i].plugin, "disk");
        ASSERT_STR(captured[i].plugin_instance, "sda");
        ASSERT(captured[i].ds_count == 2);
    }
    const metric_t *ops = find("disk_ops");
    const metric_t *oct = find("disk_octets");
    const metric_t *tim = find("disk_time");
    ASSERT(ops != NULL);
    ASSERT(oct != NULL);
    ASSERT(tim != NULL);
    /* reads_comp=100, writes_comp=200 */
    ASSERT(ops->values[0] == 100.0);
    ASSERT(ops->values[1] == 200.0);
    /* sectors_read=2000 *512 =1024000; sectors_written=4000 *512 =2048000 */
    ASSERT(oct->values[0] == 1024000.0);
    ASSERT(oct->values[1] == 2048000.0);
    /* ms_read=500, ms_write=600 */
    ASSERT(tim->values[0] == 500.0);
    ASSERT(tim->values[1] == 600.0);
}

TEST_MAIN() RUN(disk_emits_ops_octets_time_for_sda_only); TEST_RETURN()
