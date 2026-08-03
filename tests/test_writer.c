#include "minitest.h"
#include "metric.h"
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
TEST_MAIN() RUN(write_then_fetch); TEST_RETURN()
