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
    m.latency_ms     = (float)(rand() % 300);          /* 0 to 299 ms  */
    m.packet_loss    = (float)(rand() % 30) / 100.0f;  /* 0.00 to 0.29 */
    m.bandwidth_mbps = (float)(rand() % 100) + 1.0f;   /* 1 to 100 Mbps */
    return m;
}

void metrics_print(net_metrics_t m)
{
    printf("[METRICS] latency=%.1f ms  loss=%.1f%%  bandwidth=%.1f Mbps\n",
           m.latency_ms, m.packet_loss * 100.0f, m.bandwidth_mbps);
}

/* ---------------------------------------------------------------------------
 * Protocol Configuration Mutation
 * ------------------------------------------------------------------------- */

proto_config_t proto_config_default(void)
{
    proto_config_t cfg;
    cfg.window_size         = 65536;  /* 64 KB default receive window */
    cfg.retransmit_delay_ms = 200;    /* 200 ms initial RTO */
    cfg.packet_size         = 1400;   /* ~Ethernet MTU with overhead */
    cfg.congestion_window   = 1;      /* start slow */
    return cfg;
}

void mutate_proto_config(proto_config_t *cfg, net_metrics_t metrics)
{
    printf("[MUTATION] Evolving proto_config: window=%d  rto=%d ms  "
           "pkt_size=%d  cwnd=%d\n",
           cfg->window_size, cfg->retransmit_delay_ms,
           cfg->packet_size, cfg->congestion_window);

    if (metrics.latency_ms > LATENCY_HIGH_MS) {
        /* High RTT: shrink packets to avoid fragmentation and timeouts */
        cfg->packet_size -= 128;
        if (cfg->packet_size < 256) cfg->packet_size = 256;

        /* Backoff retransmit timer proportional to observed RTT */
        cfg->retransmit_delay_ms += (int)(metrics.latency_ms * 1.5f);

        printf("[MUTATION] High RTT (%.1f ms) -> "
               "packet_size=%d  rto=%d ms\n",
               metrics.latency_ms,
               cfg->packet_size, cfg->retransmit_delay_ms);
    }

    if (metrics.packet_loss > PACKET_LOSS_HIGH) {
        /* Congestion-style loss: halve cwnd and shrink segments */
        cfg->congestion_window = cfg->congestion_window > 1
                               ? cfg->congestion_window / 2 : 1;
        cfg->packet_size -= 64;
        if (cfg->packet_size < 256) cfg->packet_size = 256;

        printf("[MUTATION] Packet loss %.1f%% -> "
               "cwnd=%d  packet_size=%d\n",
               metrics.packet_loss * 100.0f,
               cfg->congestion_window, cfg->packet_size);
    }

    if (metrics.bandwidth_mbps > 50.0f) {
        /* High bandwidth: grow window aggressively and use larger frames */
        if (cfg->window_size < 262144)
            cfg->window_size *= 2;
        if (cfg->packet_size < 9000)
            cfg->packet_size = 9000;   /* jumbo frames */
        cfg->congestion_window++;

        printf("[MUTATION] High BW (%.1f Mbps) -> "
               "window=%d  packet_size=%d  cwnd=%d\n",
               metrics.bandwidth_mbps,
               cfg->window_size, cfg->packet_size, cfg->congestion_window);
    }
}

void proto_config_print(const proto_config_t *cfg)
{
    printf("[PROTO_CFG] window=%d bytes  rto=%d ms  "
           "packet_size=%d bytes  cwnd=%d\n",
           cfg->window_size, cfg->retransmit_delay_ms,
           cfg->packet_size, cfg->congestion_window);
}
