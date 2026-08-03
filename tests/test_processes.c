/** @file test_processes.c @brief processes reader: per-comm rss/cputime/count aggregation
 *
 * Expected values are DERIVED from the fixture, so a wrong stat field index or
 * statm field index fails the test (no fudged constants):
 *   fixture pid 100/101 (postgres): stat utime=100 stime=200 num_threads=5,
 *                                   statm resident=50 pages
 *   fixture pid 102 (sshd):         stat utime=10  stime=20  num_threads=3,
 *                                   statm resident=30 pages
 * rss uses pagesize via sysconf(_SC_PAGESIZE).
 */
#include "minitest.h"
#include "metric.h"
#include "reader.h"
#include <unistd.h>

static metric_t captured[64];
static char inst_buf[64][64];
static int cap_count = 0;
static int cap_cb(const metric_t *m, void *ud) {
    (void)ud;
    if (cap_count < 64) {
        captured[cap_count] = *m;
        if (m->plugin_instance) {
            strncpy(inst_buf[cap_count], m->plugin_instance, 63);
            inst_buf[cap_count][63] = '\0';
            captured[cap_count].plugin_instance = inst_buf[cap_count];
        }
    }
    cap_count++;
    return 0;
}
static const metric_t *find(const char *type, const char *inst) {
    int n = cap_count < 64 ? cap_count : 64;
    for (int i = 0; i < n; i++) {
        if (captured[i].type && strcmp(captured[i].type, type) == 0 &&
            captured[i].plugin_instance &&
            strcmp(captured[i].plugin_instance, inst) == 0) {
            return &captured[i];
        }
    }
    return NULL;
}

TEST(processes_aggregates_by_comm) {
    cap_count = 0;
    ASSERT(processes_reader.read("tests/fixtures/proc_proc", cap_cb, NULL) == 0);
    /* 2 distinct comms (postgres, sshd) x 3 metrics each = 6 emissions. */
    ASSERT(cap_count == 6);

    long pagesize = sysconf(_SC_PAGESIZE);
    ASSERT(pagesize > 0);

    /* ---- postgres: aggregated from 2 identical pids ---- */
    const metric_t *rss = find("ps_rss", "postgres");
    const metric_t *cpu = find("ps_cputime", "postgres");
    const metric_t *cnt = find("ps_count", "postgres");
    ASSERT(rss != NULL);
    ASSERT(cpu != NULL);
    ASSERT(cnt != NULL);
    ASSERT_STR(rss->plugin, "processes");
    ASSERT_STR(rss->plugin_instance, "postgres");
    ASSERT(rss->ds_count == 1);
    ASSERT(cpu->ds_count == 2);
    ASSERT(cnt->ds_count == 2);
    /* rss = 2 pids x 50 resident-pages x pagesize (statm field 2). */
    ASSERT(rss->values[0] == (double)(2 * 50) * (double)pagesize);
    /* cputime = {2 x utime, 2 x stime} = {200, 400} (stat tok 12/13 after ')'). */
    ASSERT(cpu->values[0] == 200.0);
    ASSERT(cpu->values[1] == 400.0);
    /* count = {proc_count=2, threads_sum=2 x 5=10} (num_threads tok 18 after ')'). */
    ASSERT(cnt->values[0] == 2.0);
    ASSERT(cnt->values[1] == 10.0);

    /* ---- sshd: single pid ---- */
    const metric_t *rss2 = find("ps_rss", "sshd");
    const metric_t *cpu2 = find("ps_cputime", "sshd");
    const metric_t *cnt2 = find("ps_count", "sshd");
    ASSERT(rss2 != NULL);
    ASSERT(cpu2 != NULL);
    ASSERT(cnt2 != NULL);
    ASSERT(rss2->values[0] == (double)(1 * 30) * (double)pagesize);
    ASSERT(cpu2->values[0] == 10.0);
    ASSERT(cpu2->values[1] == 20.0);
    ASSERT(cnt2->values[0] == 1.0);
    ASSERT(cnt2->values[1] == 3.0);
}

TEST_MAIN() RUN(processes_aggregates_by_comm); TEST_RETURN()
