/** @file tcpconns.c @brief TCP connections reader: per-state counts from net/tcp{,6} */
#include <stdio.h>
#include <string.h>

#include "reader.h"
#include "log.h"

/**
 * @brief TCP state index -> collectd type_instance name.
 *
 * Indices match the values in linux/tcp_states.h as they appear (as hex) in the
 * 4th column of /proc/net/tcp{,6}: 01=ESTABLISHED ... 0A=LISTEN, 0B=CLOSING.
 * Index 0 is unused (state 00 has no /proc/net/tcp representation).
 */
static const char *const TCP_STATE_NAMES[] = {
    "",            /* 0x00 (unused) */
    "ESTABLISHED", /* 0x01 */
    "SYN_SENT",    /* 0x02 */
    "SYN_RECV",    /* 0x03 */
    "FIN_WAIT1",   /* 0x04 */
    "FIN_WAIT2",   /* 0x05 */
    "TIME_WAIT",   /* 0x06 */
    "CLOSE",       /* 0x07 */
    "CLOSE_WAIT",  /* 0x08 */
    "LAST_ACK",    /* 0x09 */
    "LISTEN",      /* 0x0A */
    "CLOSING",     /* 0x0B */
};
/** One past the highest valid state index (states are 1..11). */
#define TCP_STATE_MAX ((int)(sizeof(TCP_STATE_NAMES) / sizeof(TCP_STATE_NAMES[0])))

/**
 * @brief Read <proc_base>/net/tcp and <proc_base>/net/tcp6, emit per-state counts.
 *
 * Both files share the format (one connection per data line, after a header):
 *   "  <sl>: <local_addr:port> <rem_addr:port> <st_hex> <tx_queue> ..."
 * The 4th whitespace-separated token is the socket state as a hex string
 * (01=ESTABLISHED ... 0B=LISTEN). Both address families are summed into the
 * same per-state counters (collectd's tcpconns plugin does likewise unless
 * configured to split local/remote ports).
 *
 * For every state with a non-zero count one metric is emitted
 * (plugin="tcpconns", plugin_instance=NULL), matching collectd's tcpconns
 * layout:
 *   type            = "tcp_connections"
 *   type_instance   = <STATE NAME>            (e.g. "ESTABLISHED", "TIME_WAIT")
 *   value           = count of sockets in that state
 *   -> RRD path: <base>/<host>/tcpconns/tcp_connections-ESTABLISHED.rrd
 * States with zero sockets are skipped (no empty RRD is created).
 *
 * @param proc_base prefix path (real /proc, or a fixture dir for tests).
 * @param emit      callback that receives the emitted metrics.
 * @param ud        opaque user data passed through to emit (the writer).
 * @return 0 on success, -1 if neither net/tcp nor net/tcp6 can be opened.
 */
static int tcpconns_read(const char *proc_base, metric_emit_fn emit, void *ud)
{
    long count[TCP_STATE_MAX];
    memset(count, 0, sizeof(count));

    static const char *const FILES[] = { "/net/tcp", "/net/tcp6" };
    int opened = 0;

    for (int k = 0; k < (int)(sizeof(FILES) / sizeof(FILES[0])); k++) {
        char path[1024];
        snprintf(path, sizeof(path), "%s%s", proc_base, FILES[k]);

        FILE *f = fopen(path, "r");
        if (!f) {
            /* /proc/net/tcp6 is commonly absent when IPv6 is disabled; tcp is
             * expected on any Linux host. Log both (non-fatal) and rely on the
             * "neither opened" check below to surface a hard failure. */
            log_errno("tcpconns", path);
            continue;
        }
        opened++;

        char line[512];
        int lineno = 0;
        while (fgets(line, sizeof(line), f)) {
            lineno++;
            if (lineno == 1) {
                continue;   /* header line: "sl local_address rem_address st ..." */
            }
            /* Tokens by whitespace: "0:", "<local>", "<remote>", "<st_hex>", ...
             * Three %*s skip sl/local/remote; %x reads the hex state. */
            unsigned int st = 0;
            int n = sscanf(line, " %*s %*s %*s %x", &st);
            if (n != 1) {
                continue;
            }
            if (st < 1 || st >= (unsigned int)TCP_STATE_MAX) {
                continue;
            }
            count[st]++;
        }
        fclose(f);
    }

    if (opened == 0) {
        /* Both sources unreadable: nothing to report. Readers must not fail
         * silently, so surface this as a hard (still non-fatal) error. */
        log_err("tcpconns", "neither %s/net/tcp nor %s/net/tcp6 could be opened",
                proc_base, proc_base);
        return -1;
    }

    metric_t m;
    m.plugin          = "tcpconns";
    m.plugin_instance = NULL;
    m.type            = "tcp_connections";
    m.ds_count        = 1;

    for (int i = 1; i < TCP_STATE_MAX; i++) {
        if (count[i] <= 0) {
            continue;   /* skip zero-count states: no empty RRDs */
        }
        m.type_instance = TCP_STATE_NAMES[i];
        m.values[0]     = (double)count[i];
        emit(&m, ud);
    }
    return 0;
}

const reader_t tcpconns_reader = { "tcpconns", tcpconns_read };
