#include "minitest.h"
#include "config.h"
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
TEST_MAIN() RUN(parse); RUN(missing_file_defaults); TEST_RETURN()
