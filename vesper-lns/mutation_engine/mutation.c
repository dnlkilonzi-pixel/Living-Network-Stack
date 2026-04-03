#include "mutation.h"
#include <stdio.h>
#include <stdlib.h>

/* Thresholds that trigger adaptive mutations */
#define LATENCY_HIGH_MS      150.0f
#define PACKET_LOSS_HIGH      0.10f   /* 10 % */
#define BANDWIDTH_LOW_MBPS    1.0f

void mutate(decision_t *d, net_metrics_t metrics)
{
    printf("[MUTATION] Evaluating metrics: latency=%.1f ms  "
           "loss=%.1f%%  bw=%.1f Mbps\n",
           metrics.latency_ms,
           metrics.packet_loss * 100.0f,
           metrics.bandwidth_mbps);

    if (metrics.latency_ms > LATENCY_HIGH_MS) {
        /* Switch to UDP to avoid TCP's head-of-line blocking under high RTT */
        if (!d->use_encryption) {   /* do not break security guarantee */
            printf("[MUTATION] High latency detected -> switching to UDP\n");
            d->use_udp = 1;
        } else {
            printf("[MUTATION] High latency detected, but encryption "
                   "required -> keeping TCP\n");
        }
        /* Reduce retries to cut total wait time */
        if (d->retry_count > 1) {
            d->retry_count = 1;
            printf("[MUTATION] Reduced retry count to 1\n");
        }
    }

    if (metrics.packet_loss > PACKET_LOSS_HIGH) {
        /* Increase retries to improve delivery under lossy conditions */
        int new_retries = d->retry_count + 2;
        printf("[MUTATION] High packet loss (%.1f%%) -> increasing retries "
               "from %d to %d\n",
               metrics.packet_loss * 100.0f, d->retry_count, new_retries);
        d->retry_count = new_retries;

        /* Fall back to TCP for reliable delivery when possible */
        if (!d->use_encryption) {
            /* Already might be UDP; keep but bump retries */
        }
    }

    if (metrics.bandwidth_mbps < BANDWIDTH_LOW_MBPS) {
        /* Very low bandwidth: prefer small-header UDP */
        if (!d->use_encryption) {
            printf("[MUTATION] Low bandwidth (%.1f Mbps) -> preferring UDP\n",
                   metrics.bandwidth_mbps);
            d->use_udp = 1;
        }
    }
}

net_metrics_t simulate_metrics(void)
{
    net_metrics_t m;
    /* Use fixed seeds to keep demo output deterministic */
    m.latency_ms     = (float)(rand() % 300);          /* 0 .. 299 ms */
    m.packet_loss    = (float)(rand() % 30) / 100.0f;  /* 0 .. 0.29  */
    m.bandwidth_mbps = (float)(rand() % 100) + 1.0f;   /* 1 .. 100 Mbps */
    return m;
}

void metrics_print(net_metrics_t m)
{
    printf("[METRICS] latency=%.1f ms  loss=%.1f%%  bandwidth=%.1f Mbps\n",
           m.latency_ms, m.packet_loss * 100.0f, m.bandwidth_mbps);
}
