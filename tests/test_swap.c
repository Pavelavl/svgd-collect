/** @file test_swap.c @brief swap reader: used/free bytes from meminfo */
#include "minitest.h"
#include "metric.h"
#include "reader.h"

static metric_t captured[8];
static int cap_count = 0;
static int cap_cb(const metric_t *m, void *ud) {
    (void)ud;
    if (cap_count < 8) { captured[cap_count] = *m; }
    cap_count++;
    return 0;
}
static const metric_t *find_ti(const char *ti) {
    int n = cap_count < 8 ? cap_count : 8;
    for (int i = 0; i < n; i++) {
        if (captured[i].type_instance && strcmp(captured[i].type_instance, ti) == 0) {
            return &captured[i];
        }
    }
    return NULL;
}

TEST(swap_emits_used_free_bytes) {
    cap_count = 0;
    ASSERT(swap_reader.read("tests/fixtures/proc_swap", cap_cb, NULL) == 0);
    ASSERT(cap_count == 2);
    for (int i = 0; i < 2; i++) {
        ASSERT_STR(captured[i].plugin, "swap");
        ASSERT(captured[i].plugin_instance == NULL);
        ASSERT_STR(captured[i].type, "swap");
        ASSERT(captured[i].ds_count == 1);
    }
    const metric_t *used = find_ti("used");
    const metric_t *free = find_ti("free");
    ASSERT(used != NULL);
    ASSERT(free != NULL);
    /* used = (2097152-1048576)*1024 = 1073741824; free = 1048576*1024 = 1073741824 */
    ASSERT(used->values[0] == 1073741824.0);
    ASSERT(free->values[0] == 1073741824.0);
}

TEST_MAIN() RUN(swap_emits_used_free_bytes); TEST_RETURN()
