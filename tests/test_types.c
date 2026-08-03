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
TEST(load) {
    const type_def_t *t = types_lookup("load");
    ASSERT(t != NULL && t->ds_count == 3);
    ASSERT_STR(t->ds[2].name, "longterm");
}
TEST(unknown) { ASSERT(types_lookup("nope") == NULL); }
TEST_MAIN() RUN(percent); RUN(if_octets); RUN(load); RUN(unknown); TEST_RETURN()
