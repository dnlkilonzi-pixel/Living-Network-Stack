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
