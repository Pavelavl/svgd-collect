#include "minitest.h"
#include "metric.h"
#include "types.h"
#include "writer_rrd.h"
#include <rrd.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
TEST(write_then_fetch) {
    char d[] = "/tmp/scwXXXXXX"; ASSERT(mkdtemp(d) != NULL);
    writer_t w; ASSERT(writer_init(&w, d, "localhost", "") == 0);
    metric_t m = {"cpu", NULL, "percent", "active", 1, {42.5}};
    ASSERT(writer_write(&w, &m) == 0);
    char path[512];
    snprintf(path, sizeof path, "%s/localhost/cpu/percent-active.rrd", d);
    ASSERT(access(path, F_OK) == 0);   /* file exists at drop-in path */
    time_t start = 0, end = time(NULL) + 60;
    unsigned long step = 5, ds_cnt = 0;
    char **ds_names = NULL; rrd_value_t *data = NULL;
    ASSERT(rrd_fetch_r(path, "AVERAGE", &start, &end, &step, &ds_cnt, &ds_names, &data) == 0);
    ASSERT(ds_cnt == 1);
    ASSERT_STR(ds_names[0], "value");
    rrd_freemem(ds_names); rrd_freemem(data);
}
TEST(fmt_values_precision) {
    /* if_octets is a DERIVE ds_count=2 metric; large counters (1e9+) must
     * survive formatting without scientific notation or precision loss. */
    metric_t m = {"interface", "eth0", "if_octets", "rx", 2, {1234567890.0, 9876543210.0}};
    char buf[256];
    ASSERT(fmt_values(buf, sizeof buf, types_lookup("if_octets"), &m) == 0);
    ASSERT(strstr(buf, "1234567890") != NULL);
    ASSERT(strstr(buf, "9876543210") != NULL);
    ASSERT(strchr(buf, 'e') == NULL);
    ASSERT(strchr(buf, 'E') == NULL);
}
TEST(fmt_values_huge_derive) {
    /* Regression: 64-bit DERIVE raw counters can reach ~1.8e19. %.17g would emit
     * "1.8e+19" (scientific) which rrd_update rejects; %.0f must keep it a full
     * integer with no 'e'/'E'. */
    metric_t m = {"interface", "eth0", "if_octets", "rx", 2, {1.8e19, 1.8e19}};
    char buf[256];
    ASSERT(fmt_values(buf, sizeof buf, types_lookup("if_octets"), &m) == 0);
    ASSERT(strchr(buf, 'e') == NULL);
    ASSERT(strchr(buf, 'E') == NULL);
    /* Both DS values render as the same full integer; ensure one is present. */
    const char *colon = strchr(buf, ':');  /* skip leading "N" to first value */
    ASSERT(colon != NULL);
    ASSERT(strstr(colon, "18000000000000000000") != NULL);
}
TEST(fmt_values_gauge_keeps_precision) {
    /* GAUGE must still use %.17g (round-trip-safe), not %.0f. %.0f would round
     * 42.5123... to "43"; %.17g keeps the fraction ("42.5123..."). Asserting the
     * "42.5" prefix distinguishes the two formats regardless of trailing digits. */
    metric_t m = {"cpu", "total", "percent", "active", 1, {42.5123456789}};
    char buf[256];
    ASSERT(fmt_values(buf, sizeof buf, types_lookup("percent"), &m) == 0);
    ASSERT(strstr(buf, "42.5") != NULL);   /* fraction preserved — not "43" */
    ASSERT(strstr(buf, ":43") == NULL);     /* %.0f rounding must not have happened */
}
TEST_MAIN() RUN(write_then_fetch); RUN(fmt_values_precision); RUN(fmt_values_huge_derive); RUN(fmt_values_gauge_keeps_precision); TEST_RETURN()
