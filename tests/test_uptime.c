#include "minitest.h"
#include "metric.h"
#include "reader.h"
static metric_t captured; static int cap_count = 0;
static int cap_cb(const metric_t *m, void *ud) { (void)ud; captured = *m; cap_count++; return 0; }
TEST(uptime_emits_seconds) {
    cap_count = 0;
    ASSERT(uptime_reader.read("tests/fixtures/proc", cap_cb, NULL) == 0);
    ASSERT(cap_count == 1);
    ASSERT_STR(captured.plugin, "uptime");
    ASSERT(captured.plugin_instance == NULL);
    ASSERT_STR(captured.type, "uptime");
    ASSERT(captured.type_instance == NULL);
    ASSERT(captured.ds_count == 1);
    /* raw seconds like collectd: 123456.78 (tolerance 0.01) */
    ASSERT(captured.values[0] > 123456.77 && captured.values[0] < 123456.79);
}
TEST_MAIN() RUN(uptime_emits_seconds); TEST_RETURN()
