#include "minitest.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
TEST(parse) {
    collect_config_t c;
    ASSERT(config_load(&c, "tests/fixtures/collect.json") == 0);
    ASSERT(c.interval == 10);
    ASSERT_STR(c.datadir, "/tmp/collect-test");
    ASSERT_STR(c.host, "web1");
    ASSERT_STR(c.rrdcached, "unix:/run/rrdcached.sock");
    ASSERT(c.readers_count == 2);
    ASSERT_STR(c.readers[0], "cpu");
    ASSERT_STR(c.readers[1], "memory");
}
TEST(missing_file_defaults) {
    collect_config_t c;
    ASSERT(config_load(&c, "no/such/file.json") == -1);
    ASSERT(c.interval == 5);
    ASSERT_STR(c.host, "localhost");
}
TEST(interval_zero_clamped) {
    /* Regression: atoi("0") -> 0 would make collect.c busy-loop. Must clamp to 5. */
    const char *path = "/tmp/sc_cfg_XXXXXX.json";
    char tmp[] = "/tmp/sc_cfg_XXXXXX";
    ASSERT(mkdtemp(tmp) != NULL);
    char fn[512];
    snprintf(fn, sizeof fn, "%s/collect.json", tmp);
    FILE *f = fopen(fn, "w");
    ASSERT(f != NULL);
    fputs("{ \"interval\": 0, \"datadir\": \"/tmp/x\", \"hostname\": \"h\", \"readers\": [\"cpu\"] }", f);
    fclose(f);
    collect_config_t c;
    ASSERT(config_load(&c, fn) == 0);
    ASSERT(c.interval == 5);   /* clamped from 0 */
    remove(fn);
    rmdir(tmp);
    (void)path;
}
TEST_MAIN() RUN(parse); RUN(missing_file_defaults); RUN(interval_zero_clamped); TEST_RETURN()
