#include "minitest.h"
#include "types.h"
#include "rra.h"
TEST(percent) {
    char *argv[16];
    int n = rra_args(types_lookup("percent"), argv, 16);
    ASSERT(n == 10);  /* 1 DS + 3 timespans * 3 CF (AVERAGE/MIN/MAX) */
    ASSERT_STR(argv[0], "DS:value:GAUGE:10:U:U");
    /* Per timespan, three RRAs (AVERAGE, MIN, MAX) share the same cdp_len/cdp_num:
     * the consolidation step depends only on timespan/step, not on the CF.
     * Verified by tracing collectd rra_get() (src/utils/rrdcreate/rrdcreate.c). */
    ASSERT_STR(argv[1], "RRA:AVERAGE:0.1:1:2400");   /* 1h: span bumped to 12000 */
    ASSERT_STR(argv[2], "RRA:MIN:0.1:1:2400");
    ASSERT_STR(argv[3], "RRA:MAX:0.1:1:2400");
    ASSERT_STR(argv[4], "RRA:AVERAGE:0.1:7:2469");   /* 24h: floor(86400/12000)=7, ceil(/35)=2469 */
    ASSERT_STR(argv[5], "RRA:MIN:0.1:7:2469");
    ASSERT_STR(argv[6], "RRA:MAX:0.1:7:2469");
    ASSERT_STR(argv[7], "RRA:AVERAGE:0.1:50:2420");  /* 7d: floor(604800/12000)=50, ceil(/250)=2420 */
    ASSERT_STR(argv[8], "RRA:MIN:0.1:50:2420");
    ASSERT_STR(argv[9], "RRA:MAX:0.1:50:2420");
}
TEST(if_octets) {
    char *argv[16];
    int n = rra_args(types_lookup("if_octets"), argv, 16);
    ASSERT(n == 11);  /* 2 DS + 9 RRA */
    ASSERT_STR(argv[0], "DS:rx:DERIVE:10:U:U");
    ASSERT_STR(argv[1], "DS:tx:DERIVE:10:U:U");
    ASSERT_STR(argv[2], "RRA:AVERAGE:0.1:1:2400");
    ASSERT_STR(argv[3], "RRA:MIN:0.1:1:2400");
}
TEST(too_small) {
    char *argv[2];
    ASSERT(rra_args(types_lookup("percent"), argv, 2) == -1);  /* needs 10 slots */
}
TEST_MAIN() RUN(percent); RUN(if_octets); RUN(too_small); TEST_RETURN()
