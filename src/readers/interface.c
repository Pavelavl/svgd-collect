/** @file interface.c @brief Network interface reader: rx/tx octets/packets/errors */
#include <stdio.h>
#include <string.h>

#include "reader.h"

/**
 * @brief Read <proc_base>/net/dev and emit rx/tx counters per interface.
 *
 * The first two lines of /proc/net/dev are headers and are skipped. Each data
 * line has the form:
 *   "<iface>: <rx_b> <rx_p> <rx_e> <rx_d> <rx_f> <rx_fo> <rx_mc> <multi> \
 *            <tx_b> <tx_p> <tx_e> ..."
 * so, with fields numbered from 1 after the interface name:
 *   rx_bytes=1, rx_packets=2, rx_errors=3, tx_bytes=9, tx_packets=10,
 *   tx_errors=11.
 *
 * The loopback "lo" is skipped. For every remaining interface three metrics
 * are emitted (plugin="interface", plugin_instance=<iface>, ds_count=2),
 * matching collectd's interface plugin:
 *   type="if_octets"   {rx_bytes, tx_bytes}
 *   type="if_packets"  {rx_packets, tx_packets}
 *   type="if_errors"   {rx_errors, tx_errors}
 *
 * @param proc_base prefix path (real /proc, or a fixture dir for tests).
 * @param emit      callback that receives the emitted metrics.
 * @param ud        opaque user data passed through to emit (the writer).
 * @return 0 on success, -1 if <proc_base>/net/dev cannot be opened.
 */
static int interface_read(const char *proc_base, metric_emit_fn emit, void *ud)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/net/dev", proc_base);

    FILE *f = fopen(path, "r");
    if (!f) {
        return -1;
    }

    char line[512];
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        /* skip the two header lines */
        if (lineno <= 2) {
            continue;
        }

        /* strip leading whitespace, then read iface name up to ':' */
        char *p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        char *colon = strchr(p, ':');
        if (!colon) {
            continue;
        }
        size_t len = (size_t)(colon - p);
        if (len == 0 || len >= 64) {
            continue;
        }
        char iface[64];
        memcpy(iface, p, len);
        iface[len] = '\0';

        /* skip loopback */
        if (strcmp(iface, "lo") == 0) {
            continue;
        }

        /* parse the counter fields after the colon */
        long fld[16];
        int n = sscanf(colon + 1,
                       "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
                       &fld[0], &fld[1], &fld[2], &fld[3], &fld[4],
                       &fld[5], &fld[6], &fld[7], &fld[8], &fld[9],
                       &fld[10], &fld[11], &fld[12], &fld[13], &fld[14], &fld[15]);
        if (n < 11) {
            continue;
        }
        double rx_bytes   = (double)fld[0];   /* field 1 */
        double rx_packets = (double)fld[1];   /* field 2 */
        double rx_errors  = (double)fld[2];   /* field 3 */
        double tx_bytes   = (double)fld[8];   /* field 9 */
        double tx_packets = (double)fld[9];   /* field 10 */
        double tx_errors  = (double)fld[10];  /* field 11 */

        metric_t m;
        m.plugin          = "interface";
        m.plugin_instance = iface;
        m.ds_count        = 2;

        m.type = "if_octets";
        m.type_instance = NULL;
        m.values[0] = rx_bytes;
        m.values[1] = tx_bytes;
        emit(&m, ud);

        m.type = "if_packets";
        m.values[0] = rx_packets;
        m.values[1] = tx_packets;
        emit(&m, ud);

        m.type = "if_errors";
        m.values[0] = rx_errors;
        m.values[1] = tx_errors;
        emit(&m, ud);
    }
    fclose(f);

    return 0;
}

const reader_t interface_reader = { "interface", interface_read };
