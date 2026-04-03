#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

int execute_packet(const exec_packet_t *pkt)
{
    if (!pkt) {
        fprintf(stderr, "[EXEC] execute_packet: NULL packet\n");
        return -1;
    }

    printf("[EXEC] Executing opcode=0x%02x  payload_len=%u\n",
           pkt->opcode, pkt->payload_len);

    switch (pkt->opcode) {

    case OP_NOP:
        printf("[EXEC] NOP – nothing to do\n");
        break;

    case OP_PRINT: {
        /* Treat payload as a null-terminated string (safely) */
        char buf[EXEC_PAYLOAD_MAX + 1];
        size_t copy_len = pkt->payload_len < EXEC_PAYLOAD_MAX
                        ? pkt->payload_len : EXEC_PAYLOAD_MAX;
        memcpy(buf, pkt->payload, copy_len);
        buf[copy_len] = '\0';
        printf("[EXEC] PRINT: %s\n", buf);
        break;
    }

    case OP_SUM: {
        /* Payload is a packed array of int32_t values */
        size_t count = pkt->payload_len / sizeof(int32_t);
        if (count == 0) {
            printf("[EXEC] SUM: no integers in payload\n");
            break;
        }
        int64_t total = 0;
        const int32_t *vals = (const int32_t *)pkt->payload;
        for (size_t i = 0; i < count; i++) {
            total += vals[i];
        }
        printf("[EXEC] SUM of %zu integers = %" PRId64 "\n", count, total);
        break;
    }

    case OP_ECHO:
        printf("[EXEC] ECHO: %u bytes echoed back (simulated)\n",
               pkt->payload_len);
        break;

    default:
        fprintf(stderr, "[EXEC] Unknown opcode: 0x%02x\n", pkt->opcode);
        return -1;
    }

    return 0;
}

exec_packet_t exec_make_print(const char *msg)
{
    exec_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.opcode = OP_PRINT;

    if (msg) {
        size_t len = strlen(msg);
        if (len > EXEC_PAYLOAD_MAX) len = EXEC_PAYLOAD_MAX;
        memcpy(pkt.payload, msg, len);
        pkt.payload_len = (uint16_t)len;
    }
    return pkt;
}

exec_packet_t exec_make_sum(const int32_t *values, size_t count)
{
    exec_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.opcode = OP_SUM;

    size_t max_vals = EXEC_PAYLOAD_MAX / sizeof(int32_t);
    if (count > max_vals) count = max_vals;

    memcpy(pkt.payload, values, count * sizeof(int32_t));
    pkt.payload_len = (uint16_t)(count * sizeof(int32_t));
    return pkt;
}

/* ---------------------------------------------------------------------------
 * Distributed execution context
 * ------------------------------------------------------------------------- */

void exec_context_init(exec_context_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

int execute_packet_ctx(const exec_packet_t *pkt, exec_context_t *ctx,
                       const char *node_label)
{
    if (!pkt) {
        fprintf(stderr, "[EXEC] execute_packet_ctx: NULL packet\n");
        return -1;
    }

    /* Delegate non-distributed opcodes to the standard handler */
    if (pkt->opcode != OP_SUM_PART && pkt->opcode != OP_AGGREGATE) {
        return execute_packet(pkt);
    }

    printf("[EXEC] Executing opcode=0x%02x  payload_len=%u  node=\"%s\"\n",
           pkt->opcode, pkt->payload_len,
           node_label ? node_label : "(unknown)");

    switch (pkt->opcode) {

    case OP_SUM_PART: {
        size_t i;
        size_t count;
        const int32_t *vals;
        int32_t partial;
        partial_result_t *r;

        if (!ctx) {
            fprintf(stderr, "[EXEC] OP_SUM_PART requires an exec_context_t\n");
            return -1;
        }
        if (ctx->count >= MAX_PARTIAL_RESULTS) {
            fprintf(stderr, "[EXEC] OP_SUM_PART: context is full\n");
            return -1;
        }

        count   = pkt->payload_len / sizeof(int32_t);
        vals    = (const int32_t *)pkt->payload;
        partial = 0;
        for (i = 0; i < count; i++) partial += vals[i];

        r            = &ctx->results[ctx->count++];
        r->partial_sum = partial;
        r->valid       = 1;
        if (node_label) {
            strncpy(r->node_label, node_label, sizeof(r->node_label) - 1);
            r->node_label[sizeof(r->node_label) - 1] = '\0';
        }

        printf("[EXEC] SUM_PART \"%s\": partial=%d  "
               "(total contributions so far: %d)\n",
               node_label ? node_label : "?", partial, ctx->count);
        break;
    }

    case OP_AGGREGATE: {
        int64_t total;
        int i;

        if (!ctx || ctx->count == 0) {
            printf("[EXEC] AGGREGATE: no partial results to combine\n");
            break;
        }

        total = 0;
        printf("[EXEC] AGGREGATE: combining %d partial results:\n",
               ctx->count);

        for (i = 0; i < ctx->count; i++) {
            if (!ctx->results[i].valid) continue;
            printf("  [EXEC]   node=\"%s\"  partial=%d\n",
                   ctx->results[i].node_label, ctx->results[i].partial_sum);
            total += ctx->results[i].partial_sum;
        }

        printf("[EXEC] AGGREGATE final result = %" PRId64 "\n", total);

        /* Reset context so it can be reused */
        exec_context_init(ctx);
        break;
    }

    default:
        break;
    }

    return 0;
}

exec_packet_t exec_make_sum_part(const int32_t *values, size_t count)
{
    exec_packet_t pkt;
    size_t max_vals;

    memset(&pkt, 0, sizeof(pkt));
    pkt.opcode  = OP_SUM_PART;
    max_vals    = EXEC_PAYLOAD_MAX / sizeof(int32_t);
    if (count > max_vals) count = max_vals;

    memcpy(pkt.payload, values, count * sizeof(int32_t));
    pkt.payload_len = (uint16_t)(count * sizeof(int32_t));
    return pkt;
}

exec_packet_t exec_make_aggregate(void)
{
    exec_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.opcode      = OP_AGGREGATE;
    pkt.payload_len = 0;
    return pkt;
}
