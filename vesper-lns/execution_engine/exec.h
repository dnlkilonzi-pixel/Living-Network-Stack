#ifndef EXEC_H
#define EXEC_H

#include <stddef.h>
#include <stdint.h>

/* Opcodes supported by the execution engine */
#define OP_NOP       0x00   /* No-operation */
#define OP_PRINT     0x01   /* Print payload as a string */
#define OP_SUM       0x02   /* Sum int32_t values in payload */
#define OP_ECHO      0x03   /* Echo payload back (simulated) */
#define OP_SUM_PART  0x04   /* Compute partial sum; store in context */
#define OP_AGGREGATE 0x05   /* Aggregate all partial results in context */

/* Maximum payload bytes in a single executable packet */
#define EXEC_PAYLOAD_MAX   256
#define MAX_PARTIAL_RESULTS 8

/* An executable packet sent in-network */
typedef struct {
    uint8_t  opcode;
    uint16_t payload_len;               /* actual bytes used in payload[] */
    uint8_t  payload[EXEC_PAYLOAD_MAX];
} exec_packet_t;

/* One node's contribution to a distributed computation */
typedef struct {
    int32_t partial_sum;
    char    node_label[32];
    int     valid;
} partial_result_t;

/* Shared context threaded through distributed execution across nodes */
typedef struct {
    partial_result_t results[MAX_PARTIAL_RESULTS];
    int              count;
} exec_context_t;

/* Execute an exec_packet_t (no distributed context).
 * Returns 0 on success, -1 on error. */
int execute_packet(const exec_packet_t *pkt);

/* Execute with a distributed context (required for OP_SUM_PART/OP_AGGREGATE).
 * Falls through to execute_packet() for all other opcodes.
 * Returns 0 on success, -1 on error. */
int execute_packet_ctx(const exec_packet_t *pkt, exec_context_t *ctx,
                       const char *node_label);

/* Initialise an execution context */
void exec_context_init(exec_context_t *ctx);

/* Helper: build an OP_PRINT packet from a null-terminated string */
exec_packet_t exec_make_print(const char *msg);

/* Helper: build an OP_SUM packet from an array of int32_t values */
exec_packet_t exec_make_sum(const int32_t *values, size_t count);

/* Helper: build an OP_SUM_PART packet (partial contribution to distributed sum) */
exec_packet_t exec_make_sum_part(const int32_t *values, size_t count);

/* Helper: build an OP_AGGREGATE packet */
exec_packet_t exec_make_aggregate(void);

#endif /* EXEC_H */
