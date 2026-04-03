#include "mutation.h"
#include "../rng/rng.h"
#include <stdio.h>

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
    m.latency_ms     = (float)rng_int_range(0, 299);         /* 0–299 ms  */
    m.packet_loss    = (float)rng_int_range(0, 29) / 100.0f; /* 0.00–0.29 */
    m.bandwidth_mbps = (float)rng_int_range(1, 100);          /* 1–100 Mbps */
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

/* ---------------------------------------------------------------------------
 * Backpressure / Network Stress Model
 * ------------------------------------------------------------------------- */

void stress_init(stress_state_t *s)
{
    if (!s) return;
    s->queue_bytes = 0.0f;
    s->jitter_ms   = 0.0f;
}

net_metrics_t stress_update(stress_state_t *s, size_t bytes_sent,
                             float bw_mbps, float elapsed_ms,
                             net_metrics_t base)
{
    net_metrics_t m = base;
    float drain_bytes, saturation, jitter_target, loss_add;

    if (!s) return m;

    /* How many bytes the link drained in elapsed_ms */
    drain_bytes     = (bw_mbps * 1.0e6f / 8.0f) * (elapsed_ms / 1000.0f);
    s->queue_bytes += (float)bytes_sent - drain_bytes;
    if (s->queue_bytes < 0.0f) s->queue_bytes = 0.0f;
    if (s->queue_bytes > (float)STRESS_MAX_QUEUE_BYTES)
        s->queue_bytes = (float)STRESS_MAX_QUEUE_BYTES;

    /* Normalised saturation in [0, 1] */
    saturation = s->queue_bytes / (float)STRESS_MAX_QUEUE_BYTES;

    /* Jitter: exponential moving average towards congestion target */
    jitter_target  = saturation * 50.0f;   /* up to +50 ms at full queue */
    s->jitter_ms   = s->jitter_ms * 0.9f + jitter_target * 0.1f;

    /* Apply congestion effects to metrics copy */
    m.latency_ms  += s->jitter_ms;

    loss_add       = saturation * 0.30f;   /* up to +30% loss at full queue */
    m.packet_loss += loss_add;
    if (m.packet_loss > 1.0f) m.packet_loss = 1.0f;

    /* Effective bandwidth shrinks under queue saturation */
    m.bandwidth_mbps = base.bandwidth_mbps * (1.0f - saturation * 0.8f);
    if (m.bandwidth_mbps < 0.1f) m.bandwidth_mbps = 0.1f;

    if (saturation > 0.01f) {
        printf("[STRESS] queue=%.0f bytes  sat=%.1f%%  "
               "jitter=%.1f ms  eff_bw=%.1f Mbps  eff_loss=%.1f%%\n",
               s->queue_bytes, saturation * 100.0f,
               s->jitter_ms,   m.bandwidth_mbps,
               m.packet_loss * 100.0f);
    }

    return m;
}
