#include "minitest.h"
#include "metric.h"
#include "reader.h"
static metric_t captured[8]; static int cap_count = 0;
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
TEST(memory_emits_percent_used_cached_buffered) {
    cap_count = 0;
    ASSERT(memory_reader.read("tests/fixtures/proc", cap_cb, NULL) == 0);
    ASSERT(cap_count == 3);
    for (int i = 0; i < 3; i++) {
        ASSERT_STR(captured[i].plugin, "memory");
        ASSERT(captured[i].plugin_instance == NULL);
        ASSERT_STR(captured[i].type, "percent");
        ASSERT(captured[i].ds_count == 1);
    }
    const metric_t *used     = find_ti("used");
    const metric_t *cached   = find_ti("cached");
    const metric_t *buffered = find_ti("buffered");
    ASSERT(used     != NULL);
    ASSERT(cached   != NULL);
    ASSERT(buffered != NULL);
    /* used=34.375, cached=37.5, buffered=3.125 (tolerance 0.01) */
    ASSERT(used->values[0]     > 34.365 && used->values[0]     < 34.385);
    ASSERT(cached->values[0]   > 37.490 && cached->values[0]   < 37.510);
    ASSERT(buffered->values[0] > 3.1200 && buffered->values[0] < 3.1300);
}
TEST_MAIN() RUN(memory_emits_percent_used_cached_buffered); TEST_RETURN()
