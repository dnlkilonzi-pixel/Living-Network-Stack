#ifndef OBSERVER_H
#define OBSERVER_H

#include "../mutation_engine/mutation.h"
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * System-wide statistics collected by the global observer
 * ------------------------------------------------------------------------- */

typedef struct {
    /* Routing / forwarding */
    uint32_t total_forwards;
    uint32_t successful_forwards;
    uint32_t failed_forwards;
    uint32_t total_hops;

    /* Protocol selection frequency */
    uint32_t udp_hops;
    uint32_t tcp_hops;
    uint32_t protocol_switches;  /* transitions between consecutive hops */

    /* Decision propagation */
    uint32_t propagation_rounds;

    /* Convergence: streak of consecutive hops choosing the same protocol */
    uint32_t current_streak;
    uint32_t best_streak;
    int      last_protocol;      /* 1=UDP, 0=TCP, -1=none */

    /* Metrics aggregates (for average calculations) */
    float    total_latency_ms;
    float    total_packet_loss;
    uint32_t metrics_samples;

    /* Execution engine */
    uint32_t exec_success;
    uint32_t exec_failure;
} obs_stats_t;

/* Reset all counters (call before a test run) */
void observer_init(obs_stats_t *obs);

/* Record a single hop's protocol choice and observed metrics */
void observer_record_hop(obs_stats_t *obs, int use_udp,
                         net_metrics_t metrics);

/* Record the outcome of a full network_forward() call */
void observer_record_forward(obs_stats_t *obs, int success);

/* Record a decision propagation round */
void observer_record_propagation(obs_stats_t *obs);

/* Record an execution engine outcome */
void observer_record_exec(obs_stats_t *obs, int success);

/* Print the global system observer dashboard */
void observer_report(const obs_stats_t *obs);

#endif /* OBSERVER_H */
