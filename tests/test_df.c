/** @file test_df.c @brief df reader: per-mount used/free via statvfs (denylist + statvfs skip) */
#include "minitest.h"
#include "metric.h"
#include "reader.h"

static metric_t captured[16];
/* deep storage: plugin_instance may point at reader-local storage valid only
 * during emit. type_instance points at string literals (stable, no copy). */
static char inst_buf[16][64];
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
static const metric_t *find_df(const char *tinst) {
    int n = cap_count < 16 ? cap_count : 16;
    for (int i = 0; i < n; i++) {
        if (captured[i].type_instance &&
            strcmp(captured[i].type_instance, tinst) == 0) {
            return &captured[i];
        }
    }
    return NULL;
}

TEST(df_emits_used_free_for_root_only) {
    cap_count = 0;
    ASSERT(df_reader.read("tests/fixtures/proc_df", cap_cb, NULL) == 0);
    /* Fixture mounts:
     *   rootfs / root ...            -> fstype "root" kept, statvfs("/") ok   -> emit
     *   none   /proc proc ...        -> fstype "proc" denied                  -> skip
     *   tmpfs  /tmp tmpfs ...        -> fstype "tmpfs" denied                 -> skip
     *   sda1   /mnt/nonexistent_disk ext4 -> fstype kept, statvfs ENOENT      -> skip
     * So exactly 2 metrics (used + free), both plugin_instance "root". */
    ASSERT(cap_count == 2);
    for (int i = 0; i < 2; i++) {
        ASSERT_STR(captured[i].plugin, "df");
        ASSERT_STR(captured[i].plugin_instance, "root");
        ASSERT_STR(captured[i].type, "df_complex");
        ASSERT(captured[i].ds_count == 1);
    }
    const metric_t *used = find_df("used");
    const metric_t *free = find_df("free");
    ASSERT(used != NULL);
    ASSERT(free != NULL);
    /* "/" is a real, non-empty mount -> both counters strictly positive. */
    ASSERT(used->values[0] > 0.0);
    ASSERT(free->values[0] > 0.0);
}

TEST_MAIN() RUN(df_emits_used_free_for_root_only); TEST_RETURN()
