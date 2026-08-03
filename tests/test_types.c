#include "minitest.h"
#include "types.h"
TEST(percent) {
    const type_def_t *t = types_lookup("percent");
    ASSERT(t != NULL); ASSERT_STR(t->type, "percent");
    ASSERT(t->ds_count == 1); ASSERT_STR(t->ds[0].name, "value"); ASSERT(t->ds[0].dst == DST_GAUGE);
}
TEST(if_octets) {
    const type_def_t *t = types_lookup("if_octets");
    ASSERT(t != NULL && t->ds_count == 2);
    ASSERT_STR(t->ds[0].name, "rx"); ASSERT(t->ds[0].dst == DST_DERIVE);
    ASSERT_STR(t->ds[1].name, "tx"); ASSERT(t->ds[1].dst == DST_DERIVE);
}
TEST(disk_ops) {  /* corrected: read/write (not reads/writes) */
    const type_def_t *t = types_lookup("disk_ops");
    ASSERT(t != NULL && t->ds_count == 2);
    ASSERT_STR(t->ds[0].name, "read"); ASSERT(t->ds[0].dst == DST_DERIVE);
    ASSERT_STR(t->ds[1].name, "write"); ASSERT(t->ds[1].dst == DST_DERIVE);
}
TEST(ps_cputime) {  /* corrected: user/syst (not value) */
    const type_def_t *t = types_lookup("ps_cputime");
    ASSERT(t != NULL && t->ds_count == 2);
    ASSERT_STR(t->ds[0].name, "user"); ASSERT(t->ds[0].dst == DST_DERIVE);
    ASSERT_STR(t->ds[1].name, "syst"); ASSERT(t->ds[1].dst == DST_DERIVE);
}
TEST(ps_count) {  /* corrected: processes/threads (not processes only) */
    const type_def_t *t = types_lookup("ps_count");
    ASSERT(t != NULL && t->ds_count == 2);
    ASSERT_STR(t->ds[0].name, "processes"); ASSERT(t->ds[0].dst == DST_GAUGE);
    ASSERT_STR(t->ds[1].name, "threads"); ASSERT(t->ds[1].dst == DST_GAUGE);
}
TEST(swap) {  /* corrected: single value (used/free are separate type_instances) */
    const type_def_t *t = types_lookup("swap");
    ASSERT(t != NULL && t->ds_count == 1);
    ASSERT_STR(t->ds[0].name, "value"); ASSERT(t->ds[0].dst == DST_GAUGE);
}
TEST(load) {
    const type_def_t *t = types_lookup("load");
    ASSERT(t != NULL && t->ds_count == 3);
    ASSERT_STR(t->ds[2].name, "longterm");
}
TEST(unknown) { ASSERT(types_lookup("nope") == NULL); }
TEST_MAIN()
RUN(percent); RUN(if_octets); RUN(disk_ops); RUN(ps_cputime); RUN(ps_count);
RUN(swap); RUN(load); RUN(unknown);
TEST_RETURN()
