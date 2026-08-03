/** @file test_registry.c @brief reader registry: lookup, empty config, unknown, dedup */
#include "minitest.h"
#include "registry.h"
#include "reader.h"

TEST(lookup_known_and_unknown) {
    ASSERT(registry_find("cpu")      == &cpu_reader);
    ASSERT(registry_find("thermal")  == &thermal_reader);
    ASSERT(registry_find("tcpconns") == &tcpconns_reader);
    ASSERT(registry_find("bogus")    == NULL);
    ASSERT(registry_find(NULL)       == NULL);
}

TEST(count_matches_registered_readers) {
    /* 9 original (cpu/load/uptime/memory/swap/interface/disk/df/processes)
     * + thermal + tcpconns = 11. */
    ASSERT(registry_count() == 11);
}

TEST(empty_config_enables_all_in_registry_order) {
    const reader_t *out[32];
    int n = collect_build_enabled(NULL, 0, out, 32);
    ASSERT(n == registry_count());
    ASSERT(n > 0);
    ASSERT(out[0]       == &cpu_reader);     /* first in registry order */
    ASSERT(out[n - 1]   == &tcpconns_reader); /* last registered */
    /* empty readers[] with explicit count<=0 behaves the same */
    const reader_t *out2[32];
    ASSERT(collect_build_enabled(NULL, -1, out2, 32) == registry_count());
}

TEST(unknown_reader_is_skipped) {
    const char *req[] = {"cpu", "bogus", "load"};
    const reader_t *out[8];
    int n = collect_build_enabled(req, 3, out, 8);
    ASSERT(n == 2);   /* bogus dropped, others kept in request order */
    ASSERT(out[0] == &cpu_reader);
    ASSERT(out[1] == &load_reader);
}

TEST(duplicate_reader_is_collapsed) {
    const char *req[] = {"cpu", "cpu", "cpu"};
    const reader_t *out[8];
    int n = collect_build_enabled(req, 3, out, 8);
    ASSERT(n == 1);   /* dedup by pointer identity: a reader runs once per tick */
    ASSERT(out[0] == &cpu_reader);
}

TEST(mixed_unknown_and_duplicate) {
    const char *req[] = {"thermal", "bogus", "thermal", "tcpconns", "bogus2"};
    const reader_t *out[8];
    int n = collect_build_enabled(req, 5, out, 8);
    ASSERT(n == 2);
    ASSERT(out[0] == &thermal_reader);
    ASSERT(out[1] == &tcpconns_reader);
}

TEST(max_cap_is_respected) {
    /* empty config but a tiny cap: must never write past out[max]. */
    const reader_t *out[2];
    int n = collect_build_enabled(NULL, 0, out, 2);
    ASSERT(n == 2);
}

TEST_MAIN()
    RUN(lookup_known_and_unknown);
    RUN(count_matches_registered_readers);
    RUN(empty_config_enables_all_in_registry_order);
    RUN(unknown_reader_is_skipped);
    RUN(duplicate_reader_is_collapsed);
    RUN(mixed_unknown_and_duplicate);
    RUN(max_cap_is_respected);
TEST_RETURN()
