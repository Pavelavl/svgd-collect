#include "minitest.h"
#include "metric.h"
#include "path.h"
TEST(simple) {
    metric_t m = {"cpu", NULL, "percent", "active", 1, {0}};
    char out[512];
    ASSERT(metric_to_path(out, sizeof out, "/var/rrd", "localhost", &m) == 0);
    ASSERT_STR(out, "/var/rrd/localhost/cpu/percent-active.rrd");
}
TEST(with_instances) {
    metric_t m = {"interface", "eth0", "if_octets", NULL, 2, {0}};
    char out[512];
    ASSERT(metric_to_path(out, sizeof out, "/d", "h", &m) == 0);
    ASSERT_STR(out, "/d/h/interface-eth0/if_octets.rrd");
}
TEST(null_host_seg) {
    metric_t m = {"df", "slash", "df_complex", "used", 1, {0}};
    char out[512];
    ASSERT(metric_to_path(out, sizeof out, "/d", "h", &m) == 0);
    ASSERT_STR(out, "/d/h/df-slash/df_complex-used.rrd");
}
TEST(truncation_returns_error) {
    /* Regression: a too-small buffer used to underflow size_t in the next
     * append (off bumped past n) and write OOB. Must now return -1 cleanly. */
    metric_t m = {"interface", "eth0", "if_octets", "rx", 2, {0}};
    char small[20];
    ASSERT(metric_to_path(small, sizeof small, "/var/rrd", "localhost", &m) == -1);
    /* and a host that can't even fit base/host */
    char tiny[4];
    ASSERT(metric_to_path(tiny, sizeof tiny, "/var/rrd", "localhost", &m) == -1);
}
TEST_MAIN() RUN(simple); RUN(with_instances); RUN(null_host_seg); RUN(truncation_returns_error); TEST_RETURN()
