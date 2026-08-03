/** @file processes.c @brief Processes reader: per-comm rss/cputime/count aggregation */
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include "reader.h"

/** Max distinct process names aggregated in one sample. */
#define MAX_COMMS 256
/** Linux comm is at most 15 chars (TASK_COMM_LEN-1); pad for safety. */
#define COMM_LEN 32

/**
 * @brief Per-comm accumulator.
 *
 * One slot per distinct process name read from <pid>/comm; the per-pid samples
 * from <pid>/stat and <pid>/statm are summed into these totals, then emitted.
 */
typedef struct {
    char comm[COMM_LEN];           /* process name (lookup key + emitted instance) */
    int  used;                     /* slot occupied */
    unsigned long long rss_sum;    /* sum of resident bytes (statm resident * pagesize) */
    unsigned long utime_sum;       /* sum of utime (clock ticks) */
    unsigned long stime_sum;       /* sum of stime (clock ticks) */
    int proc_count;                /* number of pids with this comm */
    int threads_sum;               /* sum of num_threads */
} comm_agg_t;

/** @return 1 if @p s is non-empty and entirely ASCII digits (a /proc/[pid] dir). */
static int is_numeric(const char *s)
{
    if (*s == '\0') {
        return 0;
    }
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
    }
    return 1;
}

/** Find the aggregation slot for @p comm, or allocate a fresh one. */
static int agg_lookup(comm_agg_t *ag, int *count, const char *comm)
{
    for (int i = 0; i < *count; i++) {
        if (ag[i].used && strcmp(ag[i].comm, comm) == 0) {
            return i;
        }
    }
    if (*count >= MAX_COMMS) {
        return -1;
    }
    int i = (*count)++;
    strncpy(ag[i].comm, comm, COMM_LEN - 1);
    ag[i].comm[COMM_LEN - 1] = '\0';
    ag[i].used        = 1;
    ag[i].rss_sum     = 0;
    ag[i].utime_sum   = 0;
    ag[i].stime_sum   = 0;
    ag[i].proc_count  = 0;
    ag[i].threads_sum = 0;
    return i;
}

/**
 * @brief Aggregate every <proc_base>/[pid] by process name and emit per-comm metrics.
 *
 * For each numeric directory <pid> under <proc_base>:
 *   - <pid>/comm  -> process name (strip trailing newline; avoids the
 *                   parens-in-stat parsing problem, since comm may contain
 *                   spaces/parens that make <pid>/stat field 2 ambiguous);
 *   - <pid>/stat   -> utime, stime, num_threads. To dodge the parenthesised
 *                   comm, the line is split on the LAST ')' and the numeric
 *                   fields after it are scanned. Per `man 5 proc`, the full
 *                   stat fields are 1:pid 2:comm 3:state 4:ppid 5:pgrp
 *                   6:session 7:tty_nr 8:tpgid 9:flags 10:minflt 11:cminflt
 *                   12:majflt 13:cmajflt 14:utime 15:stime 16:cutime
 *                   17:cstime 18:priority 19:nice 20:num_threads ...
 *                   so after ')': state is token 1, utime token 12, stime
 *                   token 13, num_threads token 18. %*s skips the state token;
 *                   17 %lu fields follow, with utime=[10], stime=[11],
 *                   num_threads=[16] (0-based into the parsed array);
 *   - <pid>/statm  -> resident pages = field 2; rss_bytes = resident * pagesize.
 *
 * Per distinct comm three metrics are emitted (plugin="processes",
 * plugin_instance=<comm>), matching collectd's processes plugin:
 *   type="ps_rss"     ds_count=1 {rss_sum}
 *   type="ps_cputime" ds_count=2 {utime_sum, stime_sum}
 *   type="ps_count"   ds_count=2 {proc_count, threads_sum}
 *
 * @param proc_base prefix path (real /proc, or a fixture dir for tests).
 * @param emit      callback that receives the emitted metrics.
 * @param ud        opaque user data passed through to emit (the writer).
 * @return 0 on success, -1 if <proc_base> cannot be opened.
 */
static int processes_read(const char *proc_base, metric_emit_fn emit, void *ud)
{
    DIR *d = opendir(proc_base);
    if (!d) {
        return -1;
    }

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096;
    }

    comm_agg_t ag[MAX_COMMS];
    int agcount = 0;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!is_numeric(de->d_name)) {
            continue;
        }
        const char *pidstr = de->d_name;
        char path[1024];

        /* ---- comm: process name (source of truth, not the stat field 2) ---- */
        snprintf(path, sizeof(path), "%s/%s/comm", proc_base, pidstr);
        FILE *cf = fopen(path, "r");
        if (!cf) {
            continue;
        }
        char comm[COMM_LEN];
        if (!fgets(comm, sizeof(comm), cf)) {
            fclose(cf);
            continue;
        }
        fclose(cf);
        /* strip trailing newline / carriage return */
        size_t len = strlen(comm);
        while (len > 0 && (comm[len - 1] == '\n' || comm[len - 1] == '\r')) {
            comm[--len] = '\0';
        }
        if (len == 0) {
            continue;
        }

        /* ---- stat: utime/stime/num_threads via split on LAST ')' ---- */
        snprintf(path, sizeof(path), "%s/%s/stat", proc_base, pidstr);
        FILE *sf = fopen(path, "r");
        if (!sf) {
            continue;
        }
        char sbuf[1024];
        if (!fgets(sbuf, sizeof(sbuf), sf)) {
            fclose(sf);
            continue;
        }
        fclose(sf);
        char *rp = strrchr(sbuf, ')');
        if (!rp) {
            continue;
        }
        rp++;  /* past the last ')' */
        /* Tokens after ')': state ppid pgrp session ttynr tpgid flags minflt
         * cminflt majflt cmajflt utime stime cutime cstime priority nice num_threads
         * %*s skips state (token 1); then 17 unsigned fields:
         *   [0]ppid [1]pgrp [2]session [3]ttynr [4]tpgid [5]flags [6]minflt
         *   [7]cminflt [8]majflt [9]cmajflt [10]utime [11]stime [12]cutime
         *   [13]cstime [14]priority [15]nice [16]num_threads */
        unsigned long fld[17];
        int n = sscanf(rp,
                       "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                       &fld[0],  &fld[1],  &fld[2],  &fld[3],  &fld[4],
                       &fld[5],  &fld[6],  &fld[7],  &fld[8],  &fld[9],
                       &fld[10], &fld[11], &fld[12], &fld[13], &fld[14],
                       &fld[15], &fld[16]);
        if (n < 17) {
            continue;
        }
        unsigned long utime       = fld[10];
        unsigned long stime       = fld[11];
        unsigned long num_threads = fld[16];

        /* ---- statm: field 2 = resident pages ---- */
        snprintf(path, sizeof(path), "%s/%s/statm", proc_base, pidstr);
        FILE *mf = fopen(path, "r");
        if (!mf) {
            continue;
        }
        char mbuf[256];
        if (!fgets(mbuf, sizeof(mbuf), mf)) {
            fclose(mf);
            continue;
        }
        fclose(mf);
        unsigned long size_pages = 0, resident_pages = 0;
        if (sscanf(mbuf, "%lu %lu", &size_pages, &resident_pages) < 2) {
            continue;
        }
        (void)size_pages;

        unsigned long long rss_bytes =
            (unsigned long long)resident_pages * (unsigned long long)page_size;

        int idx = agg_lookup(ag, &agcount, comm);
        if (idx < 0) {
            continue;
        }
        ag[idx].rss_sum     += rss_bytes;
        ag[idx].utime_sum   += utime;
        ag[idx].stime_sum   += stime;
        ag[idx].proc_count  += 1;
        ag[idx].threads_sum += (int)num_threads;
    }
    closedir(d);

    /* ---- emit per comm: ps_rss, ps_cputime, ps_count ---- */
    for (int i = 0; i < agcount; i++) {
        metric_t m;
        m.plugin          = "processes";
        m.plugin_instance = ag[i].comm;   /* stable for the function's duration */
        m.type_instance   = NULL;

        m.type      = "ps_rss";
        m.ds_count  = 1;
        m.values[0] = (double)ag[i].rss_sum;
        emit(&m, ud);

        m.type      = "ps_cputime";
        m.ds_count  = 2;
        m.values[0] = (double)ag[i].utime_sum;
        m.values[1] = (double)ag[i].stime_sum;
        emit(&m, ud);

        m.type      = "ps_count";
        m.ds_count  = 2;
        m.values[0] = (double)ag[i].proc_count;
        m.values[1] = (double)ag[i].threads_sum;
        emit(&m, ud);
    }

    return 0;
}

const reader_t processes_reader = { "processes", processes_read };
