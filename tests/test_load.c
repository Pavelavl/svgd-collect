#include "minitest.h"
#include "metric.h"
#include "reader.h"
static metric_t captured; static int cap_count = 0;
static int cap_cb(const metric_t *m, void *ud) { (void)ud; captured = *m; cap_count++; return 0; }
TEST(load_emits_three_loadavg_values) {
    cap_count = 0;
    ASSERT(load_reader.read("tests/fixtures/proc", cap_cb, NULL) == 0);
    ASSERT(cap_count == 1);
    ASSERT_STR(captured.plugin, "load");
    ASSERT(captured.plugin_instance == NULL);
    ASSERT_STR(captured.type, "load");
    ASSERT(captured.type_instance == NULL);
    ASSERT(captured.ds_count == 3);
    /* shortterm/midterm/longterm: 0.52, 0.43, 0.31 (tolerance 0.001) */
    ASSERT(captured.values[0] > 0.519 && captured.values[0] < 0.521);
    ASSERT(captured.values[1] > 0.429 && captured.values[1] < 0.431);
    ASSERT(captured.values[2] > 0.309 && captured.values[2] < 0.311);
}
TEST_MAIN() RUN(load_emits_three_loadavg_values); TEST_RETURN()
