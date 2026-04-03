#include "spec_compiler.h"
#include <string.h>
#include <stdio.h>

/* ---------------------------------------------------------------------------
 * Internal: emit one constraint into program_out
 * ------------------------------------------------------------------------- */

static void emit(spec_program_t *out,
                 const char     *name,
                 const char     *rule_text,
                 float           threshold,
                 int             chk_lat,
                 int             chk_loss,
                 int             chk_bw,
                 int             req_tcp,
                 int             req_enc)
{
    spec_constraint_t *c;

    if (out->count >= SPEC_MAX_CONSTRAINTS) return;

    c = &out->constraints[out->count];
    memset(c, 0, sizeof(*c));

    strncpy(c->name,      name,      sizeof(c->name) - 1);
    strncpy(c->rule_text, rule_text, sizeof(c->rule_text) - 1);

    c->threshold       = threshold;
    c->check_latency   = chk_lat;
    c->check_loss      = chk_loss;
    c->check_bandwidth = chk_bw;
    c->require_tcp     = req_tcp;
    c->require_encrypt = req_enc;
    c->violation_count = 0;

    out->count++;
}

/* ---------------------------------------------------------------------------
 * spec_compile — translate intent + semantics into a runnable program
 * ------------------------------------------------------------------------- */

int spec_compile(intent_t           intent,
                 intent_semantics_t semantics,
                 spec_program_t    *out)
{
    char buf[96];

    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    out->source_intent    = intent;
    out->source_semantics = semantics;

    /* ---- Rules derived from intent flags ---- */

    if (intent.flags & INTENT_LOW_LATENCY) {
        emit(out,
             "low_latency_flag",
             "latency_ms < 50.00  [from INTENT_LOW_LATENCY]",
             50.0f, 1, 0, 0, 0, 0);
    }

    if (intent.flags & INTENT_HIGH_THROUGHPUT) {
        emit(out,
             "high_throughput_flag",
             "bandwidth_mbps > 10.00  [from INTENT_HIGH_THROUGHPUT]",
             10.0f, 0, 0, 1, 0, 0);
    }

    if (intent.flags & INTENT_HIGH_SECURITY) {
        emit(out,
             "high_security_encrypt",
             "use_encryption == 1  [from INTENT_HIGH_SECURITY]",
             0.0f, 0, 0, 0, 0, 1);
    }

    /* TTL as a latency ceiling */
    if (intent.ttl_ms > 0) {
        snprintf(buf, sizeof(buf),
                 "latency_ms < %.2f  [from intent.ttl_ms]",
                 (float)intent.ttl_ms);
        emit(out, "ttl_latency_budget",
             buf, (float)intent.ttl_ms, 1, 0, 0, 0, 0);
    }

    /* ---- Rules derived from semantics ---- */

    if (semantics.max_latency_ms > 0.0f) {
        snprintf(buf, sizeof(buf),
                 "latency_ms < %.2f  [from semantics.max_latency_ms]",
                 semantics.max_latency_ms);
        emit(out, "latency_budget",
             buf, semantics.max_latency_ms, 1, 0, 0, 0, 0);
    }

    if (semantics.max_loss > 0.0f) {
        snprintf(buf, sizeof(buf),
                 "packet_loss < %.4f  [from semantics.max_loss]",
                 semantics.max_loss);
        emit(out, "loss_budget",
             buf, semantics.max_loss, 0, 1, 0, 0, 0);
    }

    if (semantics.guarantee >= GUARANTEE_AT_LEAST_ONCE) {
        const char *gname = (semantics.guarantee == GUARANTEE_EXACTLY_ONCE)
                            ? "EXACTLY_ONCE" : "AT_LEAST_ONCE";
        snprintf(buf, sizeof(buf),
                 "use_udp == 0  [delivery guarantee: %s]", gname);
        emit(out, "delivery_guarantee_tcp",
             buf, 0.0f, 0, 0, 0, 1, 0);
    }

    /* Objective-derived advisory rules (logged but not scored as hard fails) */
    switch (semantics.objective) {
    case OBJECTIVE_MINIMIZE_LATENCY:
        emit(out,
             "objective_min_latency",
             "latency_ms < 100.00  [advisory: MINIMIZE_LATENCY]",
             100.0f, 1, 0, 0, 0, 0);
        break;
    case OBJECTIVE_MINIMIZE_LOSS:
        emit(out,
             "objective_min_loss",
             "packet_loss < 0.0500  [advisory: MINIMIZE_LOSS]",
             0.05f, 0, 1, 0, 0, 0);
        break;
    case OBJECTIVE_MAXIMIZE_THROUGHPUT:
        emit(out,
             "objective_max_throughput",
             "bandwidth_mbps > 50.00  [advisory: MAXIMIZE_THROUGHPUT]",
             50.0f, 0, 0, 1, 0, 0);
        break;
    default:
        break;
    }

    return out->count;
}

/* ---------------------------------------------------------------------------
 * spec_check — evaluate one hop against the program
 * ------------------------------------------------------------------------- */

spec_check_result_t spec_check(spec_program_t   *program,
                                net_metrics_t     metrics,
                                const decision_t *decision)
{
    spec_check_result_t res;
    int i;

    memset(&res, 0, sizeof(res));
    res.passed = 1;

    if (!program || !decision) return res;

    for (i = 0; i < program->count; i++) {
        spec_constraint_t *c = &program->constraints[i];
        int failed = 0;

        if (c->check_latency   && metrics.latency_ms    > c->threshold) failed = 1;
        if (c->check_loss      && metrics.packet_loss   > c->threshold) failed = 1;
        if (c->check_bandwidth && metrics.bandwidth_mbps < c->threshold) failed = 1;
        if (c->require_tcp     && decision->use_udp)                     failed = 1;
        if (c->require_encrypt && !decision->use_encryption)             failed = 1;

        if (failed) {
            c->violation_count++;
            res.failed_idx[res.failed_count++] = i;
            res.passed = 0;
        }
    }

    return res;
}

/* ---------------------------------------------------------------------------
 * spec_assert_hop — inline assertion with stderr output on failure
 * ------------------------------------------------------------------------- */

int spec_assert_hop(spec_program_t   *program,
                    net_metrics_t     metrics,
                    const decision_t *decision,
                    const char       *node_label)
{
    spec_check_result_t res;
    int i;

    if (!program || !decision) return 1;

    res = spec_check(program, metrics, decision);

    if (!res.passed) {
        for (i = 0; i < res.failed_count; i++) {
            const spec_constraint_t *c =
                &program->constraints[res.failed_idx[i]];
            fprintf(stderr,
                    "[SPEC ASSERT] node=%-20s  FAIL  %-30s  rule: %s\n",
                    node_label ? node_label : "?",
                    c->name, c->rule_text);
        }
    }

    return res.passed;
}

/* ---------------------------------------------------------------------------
 * spec_check_session — sweep over a replay log
 * ------------------------------------------------------------------------- */

int spec_check_session(spec_program_t    *program,
                        const replay_log_t *log)
{
    int i, failed_hops = 0;

    if (!program || !log) return 0;

    for (i = 0; i < log->count; i++) {
        const replay_event_t *ev = &log->events[i];
        spec_check_result_t   res;

        if (ev->type != REPLAY_HOP) continue;

        res = spec_check(program,
                         ev->data.hop.metrics,
                         &ev->data.hop.decision);
        if (!res.passed)
            failed_hops++;
    }

    return failed_hops;
}

/* ---------------------------------------------------------------------------
 * Pretty-print helpers
 * ------------------------------------------------------------------------- */

void spec_print_program(const spec_program_t *program)
{
    int i;

    printf("\n");
    printf("============================================================\n");
    printf("  [SPEC] Compiled Constraint Program  (%d rule%s)\n",
           program->count, program->count == 1 ? "" : "s");
    printf("============================================================\n");
    printf("  Source intent flags: 0x%02x  (", program->source_intent.flags);
    if (program->source_intent.flags & INTENT_LOW_LATENCY)     printf("LOW_LATENCY ");
    if (program->source_intent.flags & INTENT_HIGH_THROUGHPUT) printf("HIGH_THROUGHPUT ");
    if (program->source_intent.flags & INTENT_HIGH_SECURITY)   printf("HIGH_SECURITY ");
    printf(")\n");
    printf("  Source guarantee : %d  max_latency=%.1f ms  max_loss=%.4f\n",
           (int)program->source_semantics.guarantee,
           program->source_semantics.max_latency_ms,
           program->source_semantics.max_loss);
    printf("  ------------------------------------------------------------\n");
    printf("  %-4s  %-30s  Rule\n", "No.", "Name");
    printf("  ------------------------------------------------------------\n");
    for (i = 0; i < program->count; i++) {
        printf("  [%02d] %-30s  %s\n",
               i,
               program->constraints[i].name,
               program->constraints[i].rule_text);
    }
    printf("============================================================\n\n");
}

void spec_print_check_result(const spec_check_result_t *result,
                              const spec_program_t      *program)
{
    int i;

    printf("[SPEC CHECK] %s",
           result->passed ? "PASS (all constraints satisfied)\n" : "FAIL\n");

    if (!result->passed && program) {
        for (i = 0; i < result->failed_count; i++) {
            int idx = result->failed_idx[i];
            printf("  [FAIL] [%02d] %s\n",
                   idx, program->constraints[idx].rule_text);
        }
    }
}

void spec_print_violations(const spec_program_t *program)
{
    int i;
    uint32_t total = 0;

    printf("\n");
    printf("============================================================\n");
    printf("  [SPEC] Cumulative Violation Summary\n");
    printf("============================================================\n");
    printf("  %-4s  %-30s  %s\n", "No.", "Name", "Violations");
    printf("  ------------------------------------------------------------\n");
    for (i = 0; i < program->count; i++) {
        printf("  [%02d] %-30s  %u\n",
               i,
               program->constraints[i].name,
               program->constraints[i].violation_count);
        total += program->constraints[i].violation_count;
    }
    printf("  ------------------------------------------------------------\n");
    printf("  Total violations: %u\n", total);
    printf("============================================================\n\n");
}
