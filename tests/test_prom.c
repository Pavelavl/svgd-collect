/** @file test_prom.c @brief Prometheus exposition formatter unit tests */
#include "minitest.h"
#include "metric.h"
#include "prom.h"
#include <stdlib.h>
#include <string.h>

/* assert haystack contains needle */
#define HAS(h, n) ASSERT(strstr((h), (n)) != NULL)

TEST(empty_render_is_safe) {
    char *out = prom_render_metrics(NULL, 0);
    ASSERT(out != NULL);
    ASSERT_STR(out, "");          /* no metrics -> empty, valid body */
    free(out);
}

TEST(unknown_metric_is_skipped) {
    metric_t m;
    m.plugin = "nonsense"; m.plugin_instance = NULL;
    m.type = "bogus";      m.type_instance = NULL;
    m.ds_count = 1;        m.values[0] = 1.0;
    char *out = prom_render_metrics(&m, 1);
    ASSERT(out != NULL);
    ASSERT_STR(out, "");          /* unknown (plugin,type) silently dropped */
    free(out);
}

TEST(cpu_percent_gauge_with_mode) {
    metric_t m;
    m.plugin = "cpu"; m.plugin_instance = "total";
    m.type = "percent"; m.type_instance = "active";
    m.ds_count = 1; m.values[0] = 42.5;
    char *out = prom_render_metrics(&m, 1);
    ASSERT(out != NULL);
    HAS(out, "# HELP svgd_cpu_percent ");
    HAS(out, "# TYPE svgd_cpu_percent gauge\n");
    HAS(out, "svgd_cpu_percent{mode=\"active\"} 42.5\n");
    /* plugin_instance "total" is a constant -> must NOT appear as a label */
    ASSERT(strstr(out, "total") == NULL);
    free(out);
}

TEST(memory_percent_emits_type_label_per_kind) {
    metric_t m[2];
    m[0].plugin = "memory"; m[0].plugin_instance = NULL;
    m[0].type = "percent"; m[0].type_instance = "used";
    m[0].ds_count = 1; m[0].values[0] = 34.375;
    m[1] = m[0];
    m[1].type_instance = "cached"; m[1].values[0] = 37.5;
    char *out = prom_render_metrics(m, 2);
    ASSERT(out != NULL);
    HAS(out, "# TYPE svgd_memory_percent gauge\n");
    /* HELP/TYPE emitted exactly once per family even with two samples */
    const char *p = out;
    int help_count = 0;
    while ((p = strstr(p, "# HELP svgd_memory_percent ")) != NULL) {
        help_count++; p++;
    }
    ASSERT(help_count == 1);
    HAS(out, "svgd_memory_percent{type=\"used\"} 34.375\n");
    HAS(out, "svgd_memory_percent{type=\"cached\"} 37.5\n");
    free(out);
}

TEST(interface_octets_counter_with_direction) {
    metric_t m;
    m.plugin = "interface"; m.plugin_instance = "eth0";
    m.type = "if_octets"; m.type_instance = NULL;
    m.ds_count = 2; m.values[0] = 1000.0; m.values[1] = 2000.0;
    char *out = prom_render_metrics(&m, 1);
    ASSERT(out != NULL);
    HAS(out, "# TYPE svgd_network_bytes_total counter\n");
    HAS(out, "svgd_network_bytes_total{device=\"eth0\",direction=\"receive\"} 1000\n");
    HAS(out, "svgd_network_bytes_total{device=\"eth0\",direction=\"transmit\"} 2000\n");
    free(out);
}

TEST(load_three_intervals_grouped) {
    metric_t m;
    m.plugin = "load"; m.plugin_instance = NULL;
    m.type = "load"; m.type_instance = NULL;
    m.ds_count = 3; m.values[0] = 0.1; m.values[1] = 0.2; m.values[2] = 0.3;
    char *out = prom_render_metrics(&m, 1);
    ASSERT(out != NULL);
    HAS(out, "# TYPE svgd_load gauge\n");
    HAS(out, "svgd_load{interval=\"1m\"} 0.1\n");
    HAS(out, "svgd_load{interval=\"5m\"} 0.2\n");
    HAS(out, "svgd_load{interval=\"15m\"} 0.3\n");
    free(out);
}

TEST(uptime_labelless_has_no_braces) {
    metric_t m;
    m.plugin = "uptime"; m.plugin_instance = NULL;
    m.type = "uptime"; m.type_instance = NULL;
    m.ds_count = 1; m.values[0] = 3600.0;
    char *out = prom_render_metrics(&m, 1);
    ASSERT(out != NULL);
    HAS(out, "# TYPE svgd_uptime_seconds gauge\n");
    HAS(out, "svgd_uptime_seconds 3600\n");
    ASSERT(strchr(out, '{') == NULL);
    free(out);
}

TEST(disk_counters_use_counter_type) {
    metric_t m;
    m.plugin = "disk"; m.plugin_instance = "sda";
    m.type = "disk_octets"; m.type_instance = NULL;
    m.ds_count = 2; m.values[0] = 512.0; m.values[1] = 1024.0;
    char *out = prom_render_metrics(&m, 1);
    ASSERT(out != NULL);
    HAS(out, "# TYPE svgd_disk_bytes_total counter\n");
    HAS(out, "svgd_disk_bytes_total{device=\"sda\",operation=\"read\"} 512\n");
    HAS(out, "svgd_disk_bytes_total{device=\"sda\",operation=\"write\"} 1024\n");
    free(out);
}

TEST(processes_rss_gauge_and_cputime_counter) {
    metric_t a, b;
    a.plugin = "processes"; a.plugin_instance = "postgres";
    a.type = "ps_rss"; a.type_instance = NULL;
    a.ds_count = 1; a.values[0] = 1048576.0;
    b.plugin = "processes"; b.plugin_instance = "postgres";
    b.type = "ps_cputime"; b.type_instance = NULL;
    b.ds_count = 2; b.values[0] = 100.0; b.values[1] = 50.0;
    metric_t arr[2] = {a, b};
    char *out = prom_render_metrics(arr, 2);
    ASSERT(out != NULL);
    HAS(out, "# TYPE svgd_process_rss_bytes gauge\n");
    HAS(out, "svgd_process_rss_bytes{process=\"postgres\"} 1048576\n");
    HAS(out, "# TYPE svgd_process_cpu_jiffies_total counter\n");
    HAS(out, "svgd_process_cpu_jiffies_total{process=\"postgres\",mode=\"user\"} 100\n");
    HAS(out, "svgd_process_cpu_jiffies_total{process=\"postgres\",mode=\"system\"} 50\n");
    free(out);
}

TEST(thermal_and_tcp_labels) {
    metric_t t, c;
    t.plugin = "thermal"; t.plugin_instance = "thermal_zone0";
    t.type = "temperature"; t.type_instance = "x86_pkg_temp";
    t.ds_count = 1; t.values[0] = 45.5;
    c.plugin = "tcpconns"; c.plugin_instance = NULL;
    c.type = "tcp_connections"; c.type_instance = "ESTABLISHED";
    c.ds_count = 1; c.values[0] = 12.0;
    metric_t arr[2] = {t, c};
    char *out = prom_render_metrics(arr, 2);
    ASSERT(out != NULL);
    HAS(out, "svgd_thermal_celsius{thermal_zone=\"thermal_zone0\",type=\"x86_pkg_temp\"} 45.5\n");
    HAS(out, "svgd_tcp_connections{state=\"ESTABLISHED\"} 12\n");
    free(out);
}

TEST(label_value_escaping) {
    /* a process name with a quote and backslash must be escaped */
    metric_t m;
    m.plugin = "processes"; m.plugin_instance = "a\"b\\c";
    m.type = "ps_rss"; m.type_instance = NULL;
    m.ds_count = 1; m.values[0] = 1.0;
    char *out = prom_render_metrics(&m, 1);
    ASSERT(out != NULL);
    HAS(out, "svgd_process_rss_bytes{process=\"a\\\"b\\\\c\"} 1\n");
    free(out);
}

TEST(help_and_type_precede_first_sample) {
    /* For each family, the HELP line must come before any sample line. */
    metric_t m;
    m.plugin = "uptime"; m.plugin_instance = NULL;
    m.type = "uptime"; m.type_instance = NULL;
    m.ds_count = 1; m.values[0] = 1.0;
    char *out = prom_render_metrics(&m, 1);
    ASSERT(out != NULL);
    const char *h = strstr(out, "# HELP svgd_uptime_seconds");
    const char *t = strstr(out, "# TYPE svgd_uptime_seconds");
    /* match the sample line by its value, not the bare family name (which also
     * appears inside the HELP/TYPE lines) */
    const char *s = strstr(out, "svgd_uptime_seconds 1\n");
    ASSERT(h != NULL && t != NULL && s != NULL);
    ASSERT(h < t && t < s);
    free(out);
}

TEST_MAIN()
RUN(empty_render_is_safe);
RUN(unknown_metric_is_skipped);
RUN(cpu_percent_gauge_with_mode);
RUN(memory_percent_emits_type_label_per_kind);
RUN(interface_octets_counter_with_direction);
RUN(load_three_intervals_grouped);
RUN(uptime_labelless_has_no_braces);
RUN(disk_counters_use_counter_type);
RUN(processes_rss_gauge_and_cputime_counter);
RUN(thermal_and_tcp_labels);
RUN(label_value_escaping);
RUN(help_and_type_precede_first_sample);
TEST_RETURN()
