/*
 * demo/main.c – Living Network Stack demonstration program
 *
 * Exercises all major subsystems:
 *   - Intent resolution
 *   - Mutation engine with simulated metrics
 *   - Protocol Virtual Machine (UDP / TCP)
 *   - Identity-based routing
 *   - Execution engine (OP_PRINT, OP_SUM)
 */

#include "../core/lns.h"
#include "../intent_engine/intent.h"
#include "../mutation_engine/mutation.h"
#include "../identity_layer/identity.h"
#include "../execution_engine/exec.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* --------------------------------------------------------------------------
 * Helper: print a section banner
 * ------------------------------------------------------------------------ */
static void banner(const char *title)
{
    printf("\n");
    printf("============================================================\n");
    printf("  %s\n", title);
    printf("============================================================\n");
}

/* --------------------------------------------------------------------------
 * Scenario 1 – Low-latency datagram send
 * ------------------------------------------------------------------------ */
static void demo_low_latency(void)
{
    banner("Scenario 1: LOW_LATENCY send");

    intent_t intent;
    intent.flags    = INTENT_LOW_LATENCY;
    intent.priority = 10;
    intent.ttl_ms   = 500;

    char payload[] = "Hello from low-latency path!";
    lns_send(payload, sizeof(payload) - 1, intent);
}

/* --------------------------------------------------------------------------
 * Scenario 2 – High-security encrypted send
 * ------------------------------------------------------------------------ */
static void demo_high_security(void)
{
    banner("Scenario 2: HIGH_SECURITY send");

    intent_t intent;
    intent.flags    = INTENT_HIGH_SECURITY;
    intent.priority = 5;
    intent.ttl_ms   = 2000;

    char payload[] = "Confidential data: AES_KEY=0xDEADBEEF";
    lns_send(payload, sizeof(payload) - 1, intent);
}

/* --------------------------------------------------------------------------
 * Scenario 3 – High-throughput bulk transfer
 * ------------------------------------------------------------------------ */
static void demo_high_throughput(void)
{
    banner("Scenario 3: HIGH_THROUGHPUT send");

    intent_t intent;
    intent.flags    = INTENT_HIGH_THROUGHPUT;
    intent.priority = 1;
    intent.ttl_ms   = 0;   /* no time limit */

    char payload[] = "Bulk data block: [1024 bytes of telemetry payload ...]";
    lns_send(payload, sizeof(payload) - 1, intent);
}

/* --------------------------------------------------------------------------
 * Scenario 4 – Simulate degraded network conditions (high latency + loss)
 * ------------------------------------------------------------------------ */
static void demo_degraded_network(void)
{
    banner("Scenario 4: Degraded network (high latency + packet loss)");

    net_metrics_t bad = {250.0f, 0.25f, 0.5f}; /* 250 ms, 25% loss, 0.5 Mbps */
    lns_set_metrics(bad);

    intent_t intent;
    intent.flags    = INTENT_LOW_LATENCY;
    intent.priority = 8;
    intent.ttl_ms   = 1000;

    char payload[] = "Resilience test under degraded conditions";
    lns_send(payload, sizeof(payload) - 1, intent);
}

/* --------------------------------------------------------------------------
 * Scenario 5 – Identity-based routing
 * ------------------------------------------------------------------------ */
static void demo_identity_routing(void)
{
    banner("Scenario 5: Identity-based routing");

    identity_init();

    /* Send to a known node */
    node_id_t target = node_id_from_label("node-beta");
    node_id_print(target);

    intent_t intent;
    intent.flags    = INTENT_HIGH_THROUGHPUT;
    intent.priority = 3;
    intent.ttl_ms   = 1500;

    char payload[] = "Routed message for node-beta";
    int rc = lns_send_to(target, payload, sizeof(payload) - 1, intent);
    printf("[DEMO] lns_send_to rc=%d\n", rc);

    /* Try an unknown node */
    node_id_t unknown = node_id_from_label("node-unknown");
    rc = lns_send_to(unknown, payload, sizeof(payload) - 1, intent);
    printf("[DEMO] lns_send_to (unknown node) rc=%d\n", rc);
}

/* --------------------------------------------------------------------------
 * Scenario 6 – Executable packets
 * ------------------------------------------------------------------------ */
static void demo_executable_packets(void)
{
    banner("Scenario 6: Executable Packets");

    /* OP_PRINT */
    exec_packet_t print_pkt = exec_make_print("In-network execution: PRINT opcode fired!");
    execute_packet(&print_pkt);

    /* OP_SUM */
    int32_t numbers[] = {10, 20, 30, 40, 50};
    exec_packet_t sum_pkt = exec_make_sum(numbers,
                                          sizeof(numbers) / sizeof(numbers[0]));
    execute_packet(&sum_pkt);

    /* OP_ECHO */
    exec_packet_t echo_pkt;
    memset(&echo_pkt, 0, sizeof(echo_pkt));
    echo_pkt.opcode      = OP_ECHO;
    const char *msg      = "echo payload";
    memcpy(echo_pkt.payload, msg, strlen(msg));
    echo_pkt.payload_len = (uint16_t)strlen(msg);
    execute_packet(&echo_pkt);

    /* OP_NOP */
    exec_packet_t nop_pkt;
    memset(&nop_pkt, 0, sizeof(nop_pkt));
    nop_pkt.opcode = OP_NOP;
    execute_packet(&nop_pkt);
}

/* --------------------------------------------------------------------------
 * Scenario 7 – Combined flags (LOW_LATENCY | HIGH_SECURITY)
 * ------------------------------------------------------------------------ */
static void demo_combined_flags(void)
{
    banner("Scenario 7: Combined LOW_LATENCY | HIGH_SECURITY (security wins)");

    /* Reset to simulated metrics */
    net_metrics_t normal = {50.0f, 0.01f, 50.0f};
    lns_set_metrics(normal);

    intent_t intent;
    intent.flags    = INTENT_LOW_LATENCY | INTENT_HIGH_SECURITY;
    intent.priority = 9;
    intent.ttl_ms   = 800;

    char payload[] = "Urgent + secure transmission";
    lns_send(payload, sizeof(payload) - 1, intent);
}

/* --------------------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------------------ */
int main(void)
{
    printf("\n");
    printf("************************************************************\n");
    printf("*        Living Network Stack (LNS) – Demo Program         *\n");
    printf("************************************************************\n");

    lns_init();

    demo_low_latency();
    demo_high_security();
    demo_high_throughput();
    demo_degraded_network();
    demo_identity_routing();
    demo_executable_packets();
    demo_combined_flags();

    lns_shutdown();

    printf("\n[DEMO] All scenarios complete.\n");
    return 0;
}
