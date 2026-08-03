/** @file disk.c @brief Disk reader: ops/octets/time per real disk from diskstats */
#include <stdio.h>
#include <string.h>
#include <regex.h>

#include "reader.h"
#include "log.h"

/**
 * @brief Read <proc_base>/diskstats and emit per-disk ops/octets/time counters.
 *
 * Each diskstats line is:
 *   "<major> <minor> <name> <reads_comp> <reads_merged> <sectors_read> \
 *    <ms_read> <writes_comp> <writes_merged> <sectors_written> <ms_write> ..."
 * The eight counters after <name> are, in order:
 *   reads_comp, reads_merged, sectors_read, ms_read,
 *   writes_comp, writes_merged, sectors_written, ms_write
 *
 * Partition lines (sda1, nvme0n1p1, ...) are filtered out — only whole disks
 * matching the regex ^(sd[a-z]+|nvme[0-9]+n[0-9]+|vd[a-z]+|mmcblk[0-9]+|
 * xvd[a-z]+|dm-[0-9]+|md[0-9]+|loop[0-9]+|sr[0-9]+)$ are kept (this mirrors
 * collectd's disk plugin, which by default also includes device-mapper,
 * software RAID, loop, and optical devices). For each, three metrics are emitted
 * (plugin="disk", plugin_instance=<name>, ds_count=2), matching collectd's
 * disk plugin:
 *   type="disk_ops"    {reads_comp, writes_comp}
 *   type="disk_octets" {sectors_read*512, sectors_written*512}   (512 B/sector)
 *   type="disk_time"   {ms_read, ms_write}
 *
 * @param proc_base prefix path (real /proc, or a fixture dir for tests).
 * @param emit      callback that receives the emitted metrics.
 * @param ud        opaque user data passed through to emit (the writer).
 * @return 0 on success, -1 if <proc_base>/diskstats cannot be opened.
 */
static int disk_read(const char *proc_base, metric_emit_fn emit, void *ud)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/diskstats", proc_base);

    FILE *f = fopen(path, "r");
    if (!f) {
        log_errno("disk", path);
        return -1;
    }

    /* Real-disk filter: whole devices only, no partitions (sda1, nvme0n1p1...).
     * Compiled once per process (REG_NOSUB — we only need a yes/no match). */
    static regex_t re;
    static int re_ready = 0;
    if (!re_ready) {
        if (regcomp(&re,
                    "^(sd[a-z]+|nvme[0-9]+n[0-9]+|vd[a-z]+|mmcblk[0-9]+|xvd[a-z]+|dm-[0-9]+|md[0-9]+|loop[0-9]+|sr[0-9]+)$",
                    REG_EXTENDED | REG_NOSUB) != 0) {
            log_err("disk", "regcomp of device-name filter failed");
            fclose(f);
            return -1;
        }
        re_ready = 1;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char name[64];
        long f1, f2, f3, f4, f5, f6, f7, f8;
        /* skip major/minor (%*d), read name, then the eight counters */
        int n = sscanf(line, "%*d %*d %63s %ld %ld %ld %ld %ld %ld %ld %ld",
                       name, &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8);
        if (n < 9) {
            continue;
        }
        /* f1=reads_comp f2=reads_merged f3=sectors_read f4=ms_read
         * f5=writes_comp f6=writes_merged f7=sectors_written f8=ms_write */
        if (regexec(&re, name, 0, NULL, 0) != 0) {
            continue;
        }

        metric_t m;
        m.plugin          = "disk";
        m.plugin_instance = name;
        m.ds_count        = 2;

        m.type = "disk_ops";
        m.type_instance = NULL;
        m.values[0] = (double)f1;                /* reads_comp  */
        m.values[1] = (double)f5;                /* writes_comp */
        emit(&m, ud);

        m.type = "disk_octets";
        m.values[0] = (double)f3 * 512.0;        /* sectors_read    * 512 */
        m.values[1] = (double)f7 * 512.0;        /* sectors_written * 512 */
        emit(&m, ud);

        m.type = "disk_time";
        m.values[0] = (double)f4;                /* ms_read  */
        m.values[1] = (double)f8;                /* ms_write */
        emit(&m, ud);
    }
    fclose(f);

    return 0;
}

const reader_t disk_reader = { "disk", disk_read };
