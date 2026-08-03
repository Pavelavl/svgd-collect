/** @file thermal.c @brief Thermal reader: degrees C from sysfs thermal_zoneN */
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "reader.h"
#include "log.h"

/**
 * @brief Read <sys>/class/thermal/thermal_zoneN/temp and emit degrees Celsius.
 *
 * Thermal data lives under sysfs (not procfs): /sys/class/thermal/thermal_zoneN.
 * Because the dispatcher always passes proc_base="/proc", that exact sentinel is
 * remapped to "/sys"; any other proc_base (a test fixture root) is used as-is,
 * so fixtures live under <base>/class/thermal/... and stay self-contained.
 *
 * For each thermal_zoneN directory two sysfs files are read:
 *   temp  - temperature in millidegrees Celsius (integer); divided by 1000.
 *   type  - the zone type (e.g. "x86_pkg_temp", "acpitz"); becomes type_instance.
 *           If the type file is missing/empty the metric is still emitted with
 *           type_instance=NULL (collectd does the same).
 *
 * One metric per zone is emitted (plugin="thermal"), matching collectd's thermal
 * plugin layout exactly:
 *   plugin_instance = <zone dir name>            (e.g. "thermal_zone0")
 *   type            = "temperature"
 *   type_instance   = <contents of the type file> (e.g. "x86_pkg_temp")
 *   value           = temp / 1000.0              (degrees Celsius)
 *   -> RRD path: <base>/<host>/thermal-thermal_zone0/temperature-x86_pkg_temp.rrd
 *
 * @param proc_base "/proc" in production (remapped to "/sys"); a fixture root
 *                  for tests.
 * @param emit      callback that receives the emitted metric.
 * @param ud        opaque user data passed through to emit (the writer).
 * @return 0 on success, -1 if the thermal class dir cannot be opened.
 */
static int thermal_read(const char *proc_base, metric_emit_fn emit, void *ud)
{
    /* /proc -> /sys remap: thermal lives under sysfs. base is kept as a const
     * pointer (no bounded intermediate) so path construction matches the other
     * readers' unbounded-source snprintf pattern. */
    const char *base = (strcmp(proc_base, "/proc") == 0) ? "/sys" : proc_base;

    char cdir[1024];
    snprintf(cdir, sizeof(cdir), "%s/class/thermal", base);

    DIR *d = opendir(cdir);
    if (!d) {
        log_errno("thermal", cdir);
        return -1;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        /* only thermal_zoneN dirs; skip thermal_cooling_deviceN and . / .. */
        if (strncmp(de->d_name, "thermal_zone", 12) != 0) {
            continue;
        }
        const char *zone = de->d_name;

        /* temp: millidegrees Celsius. */
        char tpath[1024];
        snprintf(tpath, sizeof(tpath), "%s/class/thermal/%s/temp", base, zone);
        FILE *tf = fopen(tpath, "r");
        if (!tf) {
            /* zone dir vanished or temp unreadable: log and skip this zone. */
            log_errno("thermal", tpath);
            continue;
        }
        long millideg = 0;
        int nn = fscanf(tf, "%ld", &millideg);
        fclose(tf);
        if (nn != 1) {
            log_err("thermal", "%s: cannot parse temp", tpath);
            continue;
        }

        /* type: zone type string (optional; -> type_instance). */
        char ypath[1024];
        snprintf(ypath, sizeof(ypath), "%s/class/thermal/%s/type", base, zone);
        char ztype[64];
        ztype[0] = '\0';
        FILE *yf = fopen(ypath, "r");
        if (yf) {
            if (!fgets(ztype, sizeof(ztype), yf)) {
                ztype[0] = '\0';
            }
            fclose(yf);
            /* strip trailing newline / carriage return */
            size_t L = strlen(ztype);
            while (L > 0 && (ztype[L - 1] == '\n' || ztype[L - 1] == '\r')) {
                ztype[--L] = '\0';
            }
        }

        metric_t m;
        m.plugin          = "thermal";
        m.plugin_instance = zone;                 /* thermal_zone0 */
        m.type            = "temperature";
        m.type_instance   = (ztype[0] != '\0') ? ztype : NULL;
        m.ds_count        = 1;
        m.values[0]       = (double)millideg / 1000.0;
        emit(&m, ud);
    }
    closedir(d);

    return 0;
}

const reader_t thermal_reader = { "thermal", thermal_read };
