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
TEST_MAIN() RUN(simple); RUN(with_instances); RUN(null_host_seg); TEST_RETURN()
