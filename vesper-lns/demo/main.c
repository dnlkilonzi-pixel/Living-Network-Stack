/*
 * demo/main.c – Living Network Stack demonstration program
 *
 * Exercises all major subsystems:
 *   - Intent resolution
 *   - Mutation engine with simulated metrics
 *   - Protocol Virtual Machine (UDP / TCP)
 *   - Identity-based routing
 *   - Execution engine (OP_PRINT, OP_SUM)
 *   Phase 2: Multi-node, decision propagation, proto config, trust, dist. compute
 *   Phase 3: Deterministic replay, global observer, real IPC message bus
 */

#include "../core/lns.h"
#include "../intent_engine/intent.h"
#include "../mutation_engine/mutation.h"
#include "../identity_layer/identity.h"
#include "../execution_engine/exec.h"
#include "../network/network.h"
#include "../trust_layer/trust.h"
#include "../replay/replay.h"
#include "../observer/observer.h"
#include "../bus/bus.h"
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

/* ==========================================================================
 * PHASE 2 – Advanced Scenarios
 * ======================================================================== */

/* --------------------------------------------------------------------------
 * Scenario 8 – Multi-Node Forwarding (A → B → C)
 *
 * Three nodes are created with distinct network conditions.  A message is
 * forwarded along the chain; each node independently resolves intent,
 * mutates based on its own metrics, and selects a protocol.
 * ------------------------------------------------------------------------ */
static void demo_multi_node_forwarding(void)
{
    lns_network_t net;
    int idx_a, idx_b, idx_c;
    intent_t intent;
    char payload[] = "Multi-hop message: A -> B -> C";

    banner("Scenario 8: Multi-Node Forwarding (A -> B -> C)");

    network_init(&net);

    /* Node A: healthy link */
    idx_a = network_add_node(&net, "node-A",
                             (net_metrics_t){30.0f, 0.01f, 80.0f});
    /* Node B: degraded – high latency */
    idx_b = network_add_node(&net, "node-B",
                             (net_metrics_t){200.0f, 0.05f, 20.0f});
    /* Node C: congested – high packet loss */
    idx_c = network_add_node(&net, "node-C",
                             (net_metrics_t){80.0f, 0.20f, 10.0f});

    network_add_edge(&net, idx_a, idx_b);
    network_add_edge(&net, idx_b, idx_c);

    network_print(&net);

    intent.flags    = INTENT_LOW_LATENCY;
    intent.priority = 7;
    intent.ttl_ms   = 1000;

    network_forward(&net, idx_a, idx_c, payload,
                    sizeof(payload) - 1, intent);
}

/* --------------------------------------------------------------------------
 * Scenario 9 – Decision Propagation (emergent routing convergence)
 *
 * After an initial forward, nodes broadcast their decisions to neighbours.
 * A second forward shows node B pre-adjusting its behaviour because it
 * learned what node A observed – distributed intelligence without ML.
 * ------------------------------------------------------------------------ */
static void demo_decision_propagation(void)
{
    lns_network_t net;
    int idx_a, idx_b, idx_c;
    intent_t intent;
    char payload1[] = "Round 1: initial forward";
    char payload2[] = "Round 2: post-propagation forward";

    banner("Scenario 9: Decision Propagation (nodes learn from each other)");

    network_init(&net);

    idx_a = network_add_node(&net, "node-A",
                             (net_metrics_t){250.0f, 0.15f, 5.0f});
    idx_b = network_add_node(&net, "node-B",
                             (net_metrics_t){60.0f, 0.02f, 50.0f});
    idx_c = network_add_node(&net, "node-C",
                             (net_metrics_t){40.0f, 0.01f, 90.0f});

    network_add_edge(&net, idx_a, idx_b);
    network_add_edge(&net, idx_b, idx_c);

    intent.flags    = INTENT_HIGH_THROUGHPUT;
    intent.priority = 5;
    intent.ttl_ms   = 2000;

    printf("\n--- Round 1: before propagation ---\n");
    network_forward(&net, idx_a, idx_c, payload1,
                    sizeof(payload1) - 1, intent);

    /* Propagate: A shares its decision with B; B shares with C */
    network_propagate_decisions(&net);

    printf("--- Round 2: after propagation (B now knows A's condition) ---\n");
    network_forward(&net, idx_a, idx_c, payload2,
                    sizeof(payload2) - 1, intent);
}

/* --------------------------------------------------------------------------
 * Scenario 10 – Protocol Config Evolution
 *
 * proto_config_t parameters are evolved live based on changing network
 * conditions, demonstrating "not switching protocols – evolving them".
 * ------------------------------------------------------------------------ */
static void demo_proto_config_evolution(void)
{
    proto_config_t cfg;
    net_metrics_t  conditions[3];

    banner("Scenario 10: Protocol Config Evolution");

    cfg = proto_config_default();
    printf("Initial configuration:\n");
    proto_config_print(&cfg);

    /* Condition 1: high RTT */
    conditions[0] = (net_metrics_t){300.0f, 0.02f, 30.0f};
    printf("\n[DEMO] Applying high-RTT conditions (%.1f ms):\n",
           conditions[0].latency_ms);
    metrics_print(conditions[0]);
    mutate_proto_config(&cfg, conditions[0]);
    proto_config_print(&cfg);

    /* Condition 2: packet loss */
    conditions[1] = (net_metrics_t){100.0f, 0.25f, 15.0f};
    printf("\n[DEMO] Applying high-loss conditions (%.1f%%):\n",
           conditions[1].packet_loss * 100.0f);
    metrics_print(conditions[1]);
    mutate_proto_config(&cfg, conditions[1]);
    proto_config_print(&cfg);

    /* Condition 3: high bandwidth – open up the pipeline */
    conditions[2] = (net_metrics_t){20.0f, 0.001f, 100.0f};
    printf("\n[DEMO] Applying high-BW conditions (%.1f Mbps):\n",
           conditions[2].bandwidth_mbps);
    metrics_print(conditions[2]);
    mutate_proto_config(&cfg, conditions[2]);
    proto_config_print(&cfg);
}

/* --------------------------------------------------------------------------
 * Scenario 11 – Identity + Trust Layer Routing
 *
 * Several nodes are registered with varying trust scores and capabilities.
 * The trust engine is asked to select a relay that is trusted AND has the
 * COMPUTE capability – demonstrating zero-trust + programmable routing.
 * ------------------------------------------------------------------------ */
static void demo_trust_routing(void)
{
    node_profile_t profiles[4];
    node_id_t candidates[4];
    node_id_t selected;
    int i;

    banner("Scenario 11: Identity + Trust Layer Routing");

    trust_registry_init();

    /* Profile 0: low-trust relay */
    memset(&profiles[0], 0, sizeof(profiles[0]));
    strncpy(profiles[0].label, "relay-untrusted",
            sizeof(profiles[0].label) - 1);
    profiles[0].id           = node_id_from_label("relay-untrusted");
    profiles[0].trust_score  = 20;
    profiles[0].capabilities = CAP_RELAY;
    trust_registry_add(profiles[0]);

    /* Profile 1: medium-trust compute node */
    memset(&profiles[1], 0, sizeof(profiles[1]));
    strncpy(profiles[1].label, "compute-medium",
            sizeof(profiles[1].label) - 1);
    profiles[1].id           = node_id_from_label("compute-medium");
    profiles[1].trust_score  = 55;
    profiles[1].capabilities = CAP_COMPUTE | CAP_RELAY;
    trust_registry_add(profiles[1]);

    /* Profile 2: high-trust crypto node */
    memset(&profiles[2], 0, sizeof(profiles[2]));
    strncpy(profiles[2].label, "crypto-trusted",
            sizeof(profiles[2].label) - 1);
    profiles[2].id           = node_id_from_label("crypto-trusted");
    profiles[2].trust_score  = 90;
    profiles[2].capabilities = CAP_CRYPTO | CAP_RELAY;
    trust_registry_add(profiles[2]);

    /* Profile 3: high-trust full-feature node */
    memset(&profiles[3], 0, sizeof(profiles[3]));
    strncpy(profiles[3].label, "full-node-trusted",
            sizeof(profiles[3].label) - 1);
    profiles[3].id           = node_id_from_label("full-node-trusted");
    profiles[3].trust_score  = 85;
    profiles[3].capabilities = CAP_COMPUTE | CAP_RELAY | CAP_CRYPTO
                              | CAP_STORAGE;
    trust_registry_add(profiles[3]);

    trust_registry_print();

    for (i = 0; i < 4; i++) candidates[i] = profiles[i].id;

    /* Check individual trust */
    printf("\n[DEMO] Checking trust of relay-untrusted (min=30):\n");
    trust_check(profiles[0].id, TRUST_LOW_THRESHOLD);

    printf("\n[DEMO] Checking trust of full-node-trusted (min=70):\n");
    trust_check(profiles[3].id, TRUST_HIGH_THRESHOLD);

    /* Select a trusted node with COMPUTE capability */
    printf("\n[DEMO] Selecting best COMPUTE node (min_trust=%d):\n",
           TRUST_LOW_THRESHOLD);
    if (trust_route_select(candidates, 4, CAP_COMPUTE,
                           TRUST_LOW_THRESHOLD, &selected) >= 0) {
        const node_profile_t *p = trust_lookup(selected);
        trust_print(p);
    }

    /* Penalise the crypto node (simulated misbehaviour) and re-select */
    printf("\n[DEMO] Penalising crypto-trusted (-40 trust) then re-selecting:\n");
    trust_update(profiles[2].id, -40);
    trust_registry_print();

    printf("[DEMO] Re-selecting best CRYPTO node (min_trust=60):\n");
    trust_route_select(candidates, 4, CAP_CRYPTO, 60, &selected);
}

/* --------------------------------------------------------------------------
 * Scenario 12 – Distributed Computation (A → B → C aggregate)
 *
 * Node A sends OP_SUM_PART for [1..5].
 * Node B sends OP_SUM_PART for [6..10].
 * Node C triggers OP_AGGREGATE and prints the final result (= 55).
 * This simulates network-native distributed computing.
 * ------------------------------------------------------------------------ */
static void demo_distributed_compute(void)
{
    exec_context_t ctx;
    exec_packet_t  pkt;
    int32_t        part_a[] = {1, 2, 3, 4, 5};
    int32_t        part_b[] = {6, 7, 8, 9, 10};

    banner("Scenario 12: Distributed Computation (A+B -> C aggregate)");

    exec_context_init(&ctx);

    printf("[DEMO] Node A computing partial sum of [1,2,3,4,5]:\n");
    pkt = exec_make_sum_part(part_a, sizeof(part_a) / sizeof(part_a[0]));
    execute_packet_ctx(&pkt, &ctx, "node-A");

    printf("\n[DEMO] Node B computing partial sum of [6,7,8,9,10]:\n");
    pkt = exec_make_sum_part(part_b, sizeof(part_b) / sizeof(part_b[0]));
    execute_packet_ctx(&pkt, &ctx, "node-B");

    printf("\n[DEMO] Node C aggregating results (expected total = 55):\n");
    pkt = exec_make_aggregate();
    execute_packet_ctx(&pkt, &ctx, "node-C");
}

/* ==========================================================================
 * PHASE 3 – System Becomes Real
 * ======================================================================== */

/* --------------------------------------------------------------------------
 * Scenario 13 – Deterministic Replay Engine
 *
 * A 3-node network is set up, a replay log is attached, and a multi-hop
 * forward is performed.  After the run:
 *   1.  The full trace is dumped (human-readable event log).
 *   2.  replay_run() re-executes every hop event with the same inputs and
 *       verifies the output is identical – proving determinism.
 * ------------------------------------------------------------------------ */
static void demo_deterministic_replay(void)
{
    replay_log_t  log;
    lns_network_t net;
    int idx_a, idx_b, idx_c;
    intent_t intent;
    char payload[] = "Replay test payload";

    banner("Scenario 13: Deterministic Replay Engine");

    /* Seed 42 matches lns_init() – keeps simulate_metrics() reproducible */
    replay_init(&log, 42u);
    replay_start(&log);

    network_init(&net);
    network_set_replay(&log);

    idx_a = network_add_node(&net, "replay-node-A",
                             (net_metrics_t){45.0f,  0.02f, 70.0f});
    idx_b = network_add_node(&net, "replay-node-B",
                             (net_metrics_t){180.0f, 0.08f, 15.0f});
    idx_c = network_add_node(&net, "replay-node-C",
                             (net_metrics_t){60.0f,  0.18f,  8.0f});

    network_add_edge(&net, idx_a, idx_b);
    network_add_edge(&net, idx_b, idx_c);

    intent.flags    = INTENT_HIGH_THROUGHPUT;
    intent.priority = 6;
    intent.ttl_ms   = 1500;

    network_forward(&net, idx_a, idx_c, payload,
                    sizeof(payload) - 1, intent);

    /* Also record propagation events */
    network_propagate_decisions(&net);

    replay_stop(&log);

    /* Step 1: dump the recorded trace */
    replay_dump(&log, stdout);

    /* Step 2: replay and verify determinism */
    replay_run(&log);

    /* Detach so subsequent demos are not affected */
    network_set_replay(NULL);
}

/* --------------------------------------------------------------------------
 * Scenario 14 – Global System Observer (network brain dashboard)
 *
 * Runs multiple forwards and a propagation round while the observer
 * accumulates statistics, then prints the full dashboard.
 * ------------------------------------------------------------------------ */
static void demo_global_observer(void)
{
    obs_stats_t   obs;
    lns_network_t net;
    int idx_a, idx_b, idx_c;
    intent_t intent;
    char payload[] = "Observer test payload";
    int run;

    banner("Scenario 14: Global System Observer Dashboard");

    observer_init(&obs);
    network_init(&net);
    network_set_observer(&obs);

    /* Build a 3-node mesh with varied conditions */
    idx_a = network_add_node(&net, "obs-node-A",
                             (net_metrics_t){30.0f,  0.01f, 90.0f});
    idx_b = network_add_node(&net, "obs-node-B",
                             (net_metrics_t){210.0f, 0.12f, 12.0f});
    idx_c = network_add_node(&net, "obs-node-C",
                             (net_metrics_t){70.0f,  0.22f,  5.0f});

    network_add_edge(&net, idx_a, idx_b);
    network_add_edge(&net, idx_b, idx_c);
    network_add_edge(&net, idx_a, idx_c); /* shortcut edge */

    /* Run 4 forwards with different intents to generate varied statistics */
    intent.ttl_ms   = 2000;
    intent.priority = 5;

    for (run = 0; run < 4; run++) {
        intent.flags = (run % 2 == 0) ? INTENT_LOW_LATENCY
                                      : INTENT_HIGH_THROUGHPUT;
        network_forward(&net, idx_a, idx_c, payload,
                        sizeof(payload) - 1, intent);
    }

    /* One high-security forward (will use TCP) */
    intent.flags    = INTENT_HIGH_SECURITY;
    intent.priority = 9;
    network_forward(&net, idx_a, idx_b, payload,
                    sizeof(payload) - 1, intent);

    /* Propagation round */
    network_propagate_decisions(&net);

    /* Record some exec outcomes (simulated) */
    observer_record_exec(&obs, 1);
    observer_record_exec(&obs, 1);
    observer_record_exec(&obs, 0);

    /* Print the dashboard */
    observer_report(&obs);

    /* Detach */
    network_set_observer(NULL);
}

/* --------------------------------------------------------------------------
 * Scenario 15 – Real Message Bus (IPC via UNIX socketpair)
 *
 * Three virtual nodes are registered on the bus.  Real kernel-buffered
 * datagrams are sent from node-A and node-B to node-C.  The messages are
 * serialized into a binary wire format before entering the kernel socket
 * layer and deserialized on receipt, demonstrating a real message bus layer.
 * ------------------------------------------------------------------------ */
static void demo_message_bus(void)
{
    bus_t      bus;
    bus_msg_t  received;
    char       data_a[]    = "Hello from node-A via real IPC!";
    char       data_b[]    = "Metrics update from node-B";
    /* Simulate a serialized net_metrics_t: 3 floats = 12 bytes */
    float      metrics_buf[3] = {75.0f, 0.03f, 55.0f};

    banner("Scenario 15: Real Message Bus (UNIX socketpair IPC)");

    bus_init(&bus);

    bus_register(&bus, "bus-node-A");
    bus_register(&bus, "bus-node-B");
    bus_register(&bus, "bus-node-C");

    printf("\n[DEMO] node-A sends BUS_MSG_DATA to node-C:\n");
    bus_send(&bus, "bus-node-A", "bus-node-C", BUS_MSG_DATA,
             data_a, sizeof(data_a) - 1);

    printf("\n[DEMO] node-B sends BUS_MSG_METRICS to node-C:\n");
    bus_send(&bus, "bus-node-B", "bus-node-C", BUS_MSG_METRICS,
             metrics_buf, sizeof(metrics_buf));

    printf("\n[DEMO] node-B sends BUS_MSG_DATA to node-C:\n");
    bus_send(&bus, "bus-node-B", "bus-node-C", BUS_MSG_DATA,
             data_b, sizeof(data_b) - 1);

    printf("\n[DEMO] node-A sends BUS_MSG_PING to node-C:\n");
    bus_send(&bus, "bus-node-A", "bus-node-C", BUS_MSG_PING, NULL, 0);

    printf("\n[DEMO] node-C receiving messages (100 ms timeout each):\n");

    if (bus_recv(&bus, "bus-node-C", &received, 100) == 0) {
        bus_msg_print(&received);
    } else {
        printf("[DEMO] node-C: recv #1 timed out\n");
    }

    if (bus_recv(&bus, "bus-node-C", &received, 100) == 0) {
        bus_msg_print(&received);
        /* Show the metrics payload interpreted as floats */
        if (received.msg_type == BUS_MSG_METRICS &&
            received.payload_len >= (uint32_t)(3 * sizeof(float))) {
            float lat, loss, bw;
            memcpy(&lat,  received.payload + 0, sizeof(float));
            memcpy(&loss, received.payload + 4, sizeof(float));
            memcpy(&bw,   received.payload + 8, sizeof(float));
            printf("[BUS]   decoded: latency=%.1f ms  "
                   "loss=%.1f%%  bw=%.1f Mbps\n",
                   lat, loss * 100.0f, bw);
        }
    } else {
        printf("[DEMO] node-C: recv #2 timed out\n");
    }

    if (bus_recv(&bus, "bus-node-C", &received, 100) == 0) {
        bus_msg_print(&received);
    } else {
        printf("[DEMO] node-C: recv #3 timed out\n");
    }

    if (bus_recv(&bus, "bus-node-C", &received, 100) == 0) {
        bus_msg_print(&received);
    } else {
        printf("[DEMO] node-C: recv #4 timed out\n");
    }

    /* Non-blocking recv on empty socket – should time out cleanly */
    printf("\n[DEMO] node-C non-blocking recv on empty socket:\n");
    if (bus_recv(&bus, "bus-node-C", &received, 0) == 0) {
        bus_msg_print(&received);
    } else {
        printf("[BUS]   No message available (as expected)\n");
    }

    bus_shutdown(&bus);
}

/* --------------------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------------------ */
int main(void)
{
    printf("\n");
    printf("************************************************************\n");
    printf("*        Living Network Stack (LNS) – Demo Program         *\n");
    printf("*         Phase 1 + Phase 2 + Phase 3 Scenarios            *\n");
    printf("************************************************************\n");

    lns_init();

    /* --- Phase 1 --------------------------------------------------------- */
    demo_low_latency();
    demo_high_security();
    demo_high_throughput();
    demo_degraded_network();
    demo_identity_routing();
    demo_executable_packets();
    demo_combined_flags();

    /* --- Phase 2 --------------------------------------------------------- */
    demo_multi_node_forwarding();
    demo_decision_propagation();
    demo_proto_config_evolution();
    demo_trust_routing();
    demo_distributed_compute();

    /* --- Phase 3 --------------------------------------------------------- */
    demo_deterministic_replay();
    demo_global_observer();
    demo_message_bus();

    lns_shutdown();

    printf("\n[DEMO] All scenarios complete.\n");
    return 0;
}
