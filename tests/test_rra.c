#include "minitest.h"
#include "types.h"
#include "rra.h"
TEST(percent) {
    char *argv[16];
    int n = rra_args(types_lookup("percent"), argv, 16);
    ASSERT(n == 4);  /* 1 DS + 3 RRA */
    ASSERT_STR(argv[0], "DS:value:GAUGE:10:U:U");
    /* Re-pinned to collectd's rra_get() output (step=5, rrarows=2400).
     * Verified by tracing src/utils/rrdcreate/rrdcreate.c: cdp_len/cdp_num. */
    ASSERT_STR(argv[1], "RRA:AVERAGE:0.1:1:2400");     /* 1h: span bumped to 12000 */
    ASSERT_STR(argv[2], "RRA:AVERAGE:0.1:7:2469");     /* 24h: floor(86400/12000)=7, ceil(/35)=2469 */
    ASSERT_STR(argv[3], "RRA:AVERAGE:0.1:50:2420");    /* 7d: floor(604800/12000)=50, ceil(/250)=2420 */
}
TEST(if_octets) {
    char *argv[16];
    int n = rra_args(types_lookup("if_octets"), argv, 16);
    ASSERT(n == 5);  /* 2 DS + 3 RRA */
    ASSERT_STR(argv[0], "DS:rx:DERIVE:10:U:U");
    ASSERT_STR(argv[1], "DS:tx:DERIVE:10:U:U");
    ASSERT_STR(argv[2], "RRA:AVERAGE:0.1:1:2400");
}
TEST(too_small) {
    char *argv[2];
    ASSERT(rra_args(types_lookup("percent"), argv, 2) == -1);  /* needs 4 slots */
}
TEST_MAIN() RUN(percent); RUN(if_octets); RUN(too_small); TEST_RETURN()
