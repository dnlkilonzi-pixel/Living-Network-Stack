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
    /* Convergence timing (wall-clock, CLOCK_MONOTONIC milliseconds) */
    uint32_t convergence_count;       /* how many times the network re-converged */
    uint64_t convergence_time_ms;     /* total accumulated convergence time (ms) */
    int64_t  convergence_start_ms;    /* -1 = not timing; >=0 = epoch of last switch */

} obs_stats_t;

/* Number of consecutive same-protocol hops that signals convergence */
#define OBSERVER_CONVERGENCE_STREAK  3

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

/* ---------------------------------------------------------------------------
 * Observability Query Language
 *
 * A simple predicate-based query engine that scans a replay_log_t (the
 * machine-readable event stream) and computes aggregated metrics matching
 * the supplied filter criteria.
 *
 * Example usage:
 *   obs_query_t  q  = { .filter_proto = 1 };   // UDP only
 *   obs_query_result_t r;
 *   observer_query(&session->replay, &q, &r);
 *   printf("UDP hops: %u  avg_latency: %.1f ms\n",
 *          r.matched_hops, r.avg_latency_ms);
 * ------------------------------------------------------------------------- */

/* Include replay for replay_log_t */
#include "../replay/replay.h"

/* Filter protocol values */
#define OBS_PROTO_ANY  (-1)   /* no protocol filter */
#define OBS_PROTO_TCP   (0)
#define OBS_PROTO_UDP   (1)

typedef struct {
    int  filter_proto;      /* OBS_PROTO_ANY / OBS_PROTO_TCP / OBS_PROTO_UDP */
    char filter_node[32];   /* empty string = match all nodes */
    int  hop_min;           /* 0 = no lower bound on hop number */
    int  hop_max;           /* 0 = no upper bound on hop number */
} obs_query_t;

typedef struct {
    uint32_t matched_hops;      /* number of HOP events matched             */
    float    avg_latency_ms;    /* average observed latency (ms)             */
    float    avg_loss;          /* average packet loss ratio [0, 1]          */
    float    avg_bandwidth_mbps;/* average bandwidth (Mbps)                  */
    uint32_t udp_count;         /* matched hops that used UDP                */
    uint32_t tcp_count;         /* matched hops that used TCP                */
    uint32_t causal_hop_count;  /* matched hops with a non-root causal parent*/
} obs_query_result_t;

/* Scan replay log events and fill result according to query predicates.
 * result is zeroed before scanning; always safe to call with an empty log. */
void observer_query(const replay_log_t *log,
                    const obs_query_t  *query,
                    obs_query_result_t *result);

/* Pretty-print a query result */
void observer_query_print(const obs_query_result_t *result);

#endif /* OBSERVER_H */
