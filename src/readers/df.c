/** @file df.c @brief Disk-usage reader: per-mount used/free via statvfs */
#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>

#include "reader.h"

/**
 * @brief Filesystem types to ignore (pseudo / non-disk filesystems).
 *
 * Matches collectd's df plugin defaults (the partition types it does not
 * report): these never correspond to a backing block device with meaningful
 * used/free counters.
 */
static const char *const FSTYPE_DENYLIST[] = {
    "proc", "sysfs", "tmpfs", "devtmpfs", "devpts", "cgroup", "cgroup2",
    "mqueue", "hugetlbfs", "fusectl", "debugfs", "configfs", "pstore",
    "bpf", "tracefs",
};
#define FSTYPE_DENYLIST_N (sizeof(FSTYPE_DENYLIST) / sizeof(FSTYPE_DENYLIST[0]))

/** @return 1 if @p fstype is in the denylist. */
static int fstype_denied(const char *fstype)
{
    for (size_t i = 0; i < FSTYPE_DENYLIST_N; i++) {
        if (strcmp(fstype, FSTYPE_DENYLIST[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Turn a mount path into a collectd-style plugin_instance.
 *
 * Mirrors collectd's df plugin: the leading root slash is dropped and every
 * remaining '/' becomes '-'. A bare "/" therefore collapses to "" and is
 * normalized to "root" (an empty instance would be ambiguous across mounts).
 *   "/"       -> "root"
 *   "/mnt/c"  -> "mnt-c"
 *   "/boot"   -> "boot"
 */
static void mount_to_instance(const char *mount, char *out, size_t outsz)
{
    const char *src = mount;
    /* drop the leading root slash so "/mnt/c" -> "mnt-c", not "-mnt-c" */
    if (*src == '/') {
        src++;
    }
    size_t o = 0;
    for (size_t i = 0; src[i] && o + 1 < outsz; i++) {
        out[o++] = (src[i] == '/') ? '-' : src[i];
    }
    out[o] = '\0';
    /* bare "/" -> "" -> "root" */
    if (o == 0) {
        strncpy(out, "root", outsz - 1);
        out[outsz - 1] = '\0';
    }
}

/**
 * @brief Read <proc_base>/mounts and emit per-mount used/free df_complex metrics.
 *
 * Each mounts line is: "<dev> <mount> <fstype> <opts> <dump> <pass>".
 * Pseudo filesystems (in FSTYPE_DENYLIST) and mounts whose path is not
 * statvfs()-able are skipped. For each remaining mount two metrics are emitted
 * (plugin="df", plugin_instance=<mount-as-instance>, type="df_complex",
 * ds_count=1), matching collectd's df plugin:
 *   type_instance="used" value = (f_blocks - f_bfree) * f_frsize
 *   type_instance="free" value =  f_bavail            * f_frsize
 * (used counts only blocks holding data, not reserved-for-root space,
 *  matching collectd's df_complex-used. collectd additionally emits
 *  df_complex-reserved = (f_bfree - f_bavail) * f_frsize, which svgd-collect
 *  does not produce. f_bavail = blocks available to unprivileged users.)
 *
 * @param proc_base prefix path (real /proc, or a fixture dir for tests).
 * @param emit      callback that receives the emitted metrics.
 * @param ud        opaque user data passed through to emit (the writer).
 * @return 0 on success, -1 if <proc_base>/mounts cannot be opened.
 */
static int df_read(const char *proc_base, metric_emit_fn emit, void *ud)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/mounts", proc_base);

    FILE *f = fopen(path, "r");
    if (!f) {
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char dev[256], mount[256], fstype[64], opts[256];
        /* dev mount fstype opts [dump pass] — first four fields suffice */
        int n = sscanf(line, "%255s %255s %63s %255s", dev, mount, fstype, opts);
        if (n < 4) {
            continue;
        }
        (void)dev;
        (void)opts;

        if (fstype_denied(fstype)) {
            continue;
        }

        struct statvfs s;
        if (statvfs(mount, &s) != 0) {
            /* unmounted / inaccessible / stale mount point */
            continue;
        }

        /* used = data blocks only (reserved-for-root excluded), like collectd */
        unsigned long long used = (unsigned long long)(s.f_blocks - s.f_bfree)
                                  * (unsigned long long)s.f_frsize;
        unsigned long long freeb = (unsigned long long)s.f_bavail
                                   * (unsigned long long)s.f_frsize;

        char inst[256];
        mount_to_instance(mount, inst, sizeof(inst));

        metric_t m;
        m.plugin          = "df";
        m.plugin_instance = inst;   /* loop-local storage, valid during emit */
        m.type            = "df_complex";
        m.ds_count        = 1;

        m.type_instance = "used";
        m.values[0] = (double)used;
        emit(&m, ud);

        m.type_instance = "free";
        m.values[0] = (double)freeb;
        emit(&m, ud);
    }
    fclose(f);

    return 0;
}

const reader_t df_reader = { "df", df_read };
