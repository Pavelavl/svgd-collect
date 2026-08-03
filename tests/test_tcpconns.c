/** @file test_tcpconns.c @brief tcpconns reader: per-state counts from net/tcp{,6} */
#include "minitest.h"
#include "metric.h"
#include "reader.h"

static metric_t captured[16];
static char ti_buf[16][64];
static int cap_count = 0;
static int cap_cb(const metric_t *m, void *ud) {
    (void)ud;
    if (cap_count < 16) {
        captured[cap_count] = *m;
        if (m->type_instance) {
            strncpy(ti_buf[cap_count], m->type_instance, 63);
            ti_buf[cap_count][63] = '\0';
            captured[cap_count].type_instance = ti_buf[cap_count];
        }
    }
    cap_count++;
    return 0;
}
static const metric_t *find_state(const char *st) {
    int n = cap_count < 16 ? cap_count : 16;
    for (int i = 0; i < n; i++) {
        if (captured[i].type_instance &&
            strcmp(captured[i].type_instance, st) == 0) {
            return &captured[i];
        }
    }
    return NULL;
}

TEST(tcpconns_counts_states_across_tcp_and_tcp6) {
    cap_count = 0;
    /* Fixture (tests/fixtures/proc_net/net/tcp{,6}):
     *   tcp  : LISTEN=2, TIME_WAIT=1, ESTABLISHED=1
     *   tcp6 : LISTEN=1, ESTABLISHED=1
     * Totals: ESTABLISHED=2, LISTEN=3, TIME_WAIT=1. */
    ASSERT(tcpconns_reader.read("tests/fixtures/proc_net", cap_cb, NULL) == 0);
    /* only non-zero states are emitted -> 3 metrics */
    ASSERT(cap_count == 3);

    const metric_t *est = find_state("ESTABLISHED");
    const metric_t *lst = find_state("LISTEN");
    const metric_t *tw  = find_state("TIME_WAIT");
    ASSERT(est != NULL);
    ASSERT(lst != NULL);
    ASSERT(tw  != NULL);

    ASSERT(est->values[0] == 2.0);
    ASSERT(lst->values[0] == 3.0);
    ASSERT(tw->values[0]  == 1.0);

    for (int i = 0; i < 3; i++) {
        ASSERT_STR(captured[i].plugin, "tcpconns");
        ASSERT_STR(captured[i].type, "tcp_connections");
        ASSERT(captured[i].plugin_instance == NULL);
        ASSERT(captured[i].ds_count == 1);
    }
    /* zero-count states must NOT be emitted (no empty RRDs created) */
    ASSERT(find_state("SYN_SENT")   == NULL);
    ASSERT(find_state("CLOSE_WAIT") == NULL);
}

TEST(tcpconns_missing_files_returns_error) {
    cap_count = 0;
    /* neither net/tcp nor net/tcp6 exists -> -1 (logged, non-fatal) */
    ASSERT(tcpconns_reader.read("tests/fixtures/no_such_net", cap_cb, NULL) == -1);
    ASSERT(cap_count == 0);
}

TEST_MAIN() RUN(tcpconns_counts_states_across_tcp_and_tcp6); RUN(tcpconns_missing_files_returns_error); TEST_RETURN()
