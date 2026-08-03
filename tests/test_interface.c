/** @file test_interface.c @brief interface reader: rx/tx octets/packets/errors per iface */
#include "minitest.h"
#include "metric.h"
#include "reader.h"

static metric_t captured[16];
static char inst_buf[16][64];   /* deep storage: plugin_instance may point at
                                 * reader-local storage valid only during emit */
static int cap_count = 0;
static int cap_cb(const metric_t *m, void *ud) {
    (void)ud;
    if (cap_count < 16) {
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
    int n = cap_count < 16 ? cap_count : 16;
    for (int i = 0; i < n; i++) {
        if (captured[i].type && strcmp(captured[i].type, type) == 0 &&
            captured[i].plugin_instance &&
            strcmp(captured[i].plugin_instance, inst) == 0) {
            return &captured[i];
        }
    }
    return NULL;
}

TEST(interface_emits_octets_packets_errors_for_eth0) {
    cap_count = 0;
    ASSERT(interface_reader.read("tests/fixtures/proc_net", cap_cb, NULL) == 0);
    /* only eth0 — lo is skipped */
    ASSERT(cap_count == 3);
    for (int i = 0; i < 3; i++) {
        ASSERT_STR(captured[i].plugin, "interface");
        ASSERT_STR(captured[i].plugin_instance, "eth0");
        ASSERT(captured[i].ds_count == 2);
        /* per-iface metrics carry no type_instance (no per-subpath split) */
        ASSERT(captured[i].type_instance == NULL);
    }
    const metric_t *oct   = find("if_octets",  "eth0");
    const metric_t *pack  = find("if_packets", "eth0");
    const metric_t *err   = find("if_errors",  "eth0");
    ASSERT(oct  != NULL);
    ASSERT(pack != NULL);
    ASSERT(err  != NULL);
    /* rx_bytes(1)=1000, tx_bytes(9)=2000 */
    ASSERT(oct->values[0] == 1000.0);
    ASSERT(oct->values[1] == 2000.0);
    /* rx_packets(2)=100, tx_packets(10)=150 */
    ASSERT(pack->values[0] == 100.0);
    ASSERT(pack->values[1] == 150.0);
    /* rx_errors(3)=5, tx_errors(11)=2 */
    ASSERT(err->values[0] == 5.0);
    ASSERT(err->values[1] == 2.0);
}

TEST_MAIN() RUN(interface_emits_octets_packets_errors_for_eth0); TEST_RETURN()
