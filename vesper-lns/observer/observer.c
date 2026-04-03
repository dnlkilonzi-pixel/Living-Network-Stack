#include "observer.h"
#include <stdio.h>
#include <string.h>

void observer_init(obs_stats_t *obs)
{
    memset(obs, 0, sizeof(*obs));
    obs->last_protocol = -1;
    printf("[OBSERVER] Counters reset\n");
}

void observer_record_hop(obs_stats_t *obs, int use_udp,
                         net_metrics_t metrics)
{
    int prev;

    obs->total_hops++;

    if (use_udp) obs->udp_hops++;
    else         obs->tcp_hops++;

    /* Track protocol switches and convergence streak */
    prev = obs->last_protocol;
    if (prev != -1 && prev != use_udp) {
        obs->protocol_switches++;
        obs->current_streak = 1;
    } else {
        obs->current_streak++;
    }
    if (obs->current_streak > obs->best_streak) {
        obs->best_streak = obs->current_streak;
    }
    obs->last_protocol = use_udp;

    /* Accumulate metrics for averages */
    obs->total_latency_ms  += metrics.latency_ms;
    obs->total_packet_loss += metrics.packet_loss;
    obs->metrics_samples++;
}

void observer_record_forward(obs_stats_t *obs, int success)
{
    obs->total_forwards++;
    if (success) obs->successful_forwards++;
    else         obs->failed_forwards++;
}

void observer_record_propagation(obs_stats_t *obs)
{
    obs->propagation_rounds++;
}

void observer_record_exec(obs_stats_t *obs, int success)
{
    if (success) obs->exec_success++;
    else         obs->exec_failure++;
}

void observer_report(const obs_stats_t *obs)
{
    float avg_latency  = 0.0f;
    float avg_loss     = 0.0f;
    float success_rate = 0.0f;
    float udp_pct      = 0.0f;
    float stability    = 0.0f;

    if (obs->metrics_samples > 0) {
        avg_latency = obs->total_latency_ms  / (float)obs->metrics_samples;
        avg_loss    = obs->total_packet_loss / (float)obs->metrics_samples;
    }
    if (obs->total_forwards > 0) {
        success_rate = 100.0f * (float)obs->successful_forwards
                              / (float)obs->total_forwards;
    }
    if (obs->total_hops > 0) {
        udp_pct   = 100.0f * (float)obs->udp_hops / (float)obs->total_hops;
        /* routing stability: fraction of hops that did NOT switch protocol */
        stability = (obs->total_hops > 1)
                  ? 100.0f * (1.0f - (float)obs->protocol_switches
                                   / (float)(obs->total_hops - 1))
                  : 100.0f;
    }

    printf("\n");
    printf("============================================================\n");
    printf("  [OBSERVER] Global System Dashboard\n");
    printf("============================================================\n");
    printf("  Forwarding\n");
    printf("    Total forwards        : %u\n",  obs->total_forwards);
    printf("    Successful            : %u\n",  obs->successful_forwards);
    printf("    Failed                : %u\n",  obs->failed_forwards);
    printf("    Success rate          : %.1f%%\n", success_rate);
    printf("\n");
    printf("  Routing\n");
    printf("    Total hops            : %u\n",  obs->total_hops);
    printf("    UDP hops              : %u (%.1f%%)\n",
           obs->udp_hops, udp_pct);
    printf("    TCP hops              : %u (%.1f%%)\n",
           obs->tcp_hops, 100.0f - udp_pct);
    printf("    Protocol switches     : %u\n",  obs->protocol_switches);
    printf("    Routing stability     : %.1f%%\n", stability);
    printf("    Best stable streak    : %u hops\n", obs->best_streak);
    printf("\n");
    printf("  Convergence\n");
    printf("    Propagation rounds    : %u\n",  obs->propagation_rounds);
    printf("    Current streak        : %u hops on same protocol\n",
           obs->current_streak);
    printf("\n");
    printf("  Network Conditions (averages over %u samples)\n",
           obs->metrics_samples);
    printf("    Avg latency           : %.1f ms\n",  avg_latency);
    printf("    Avg packet loss       : %.1f%%\n",   avg_loss * 100.0f);
    printf("\n");
    printf("  Execution Engine\n");
    printf("    Exec successes        : %u\n",  obs->exec_success);
    printf("    Exec failures         : %u\n",  obs->exec_failure);
    printf("============================================================\n\n");
}
