#include "minitest.h"
#include "metric.h"
#include "reader.h"
static metric_t captured; static int cap_count = 0;
static int cap_cb(const metric_t *m, void *ud) { (void)ud; captured = *m; cap_count++; return 0; }
TEST(cpu_percent_delta) {
    cap_count = 0;
    /* first sample primes prev state; emits nothing */
    ASSERT(cpu_reader.read("tests/fixtures/proc1", cap_cb, NULL) == 0);
    ASSERT(cap_count == 0);
    /* second sample: emits percent (delta_busy=100, delta_total=1000 -> 20%) */
    ASSERT(cpu_reader.read("tests/fixtures/proc2", cap_cb, NULL) == 0);
    ASSERT(cap_count == 1);
    ASSERT_STR(captured.plugin, "cpu");
    /* drop-in: collectd cpu aggregation sets plugin_instance="total", so svgd
     * reads cpu-total/percent-active.rrd. Must NOT be NULL. */
    ASSERT(captured.plugin_instance != NULL);
    ASSERT_STR(captured.plugin_instance, "total");
    ASSERT_STR(captured.type, "percent");
    ASSERT_STR(captured.type_instance, "active");
    ASSERT(captured.ds_count == 1);
    ASSERT(captured.values[0] > 19.9 && captured.values[0] < 20.1);
}
TEST_MAIN() RUN(cpu_percent_delta); TEST_RETURN()
