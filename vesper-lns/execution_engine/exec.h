#ifndef EXEC_H
#define EXEC_H

#include <stddef.h>
#include <stdint.h>

/* Opcodes supported by the execution engine */
#define OP_NOP    0x00   /* No-operation */
#define OP_PRINT  0x01   /* Print payload as a string */
#define OP_SUM    0x02   /* Sum int32_t values in payload */
#define OP_ECHO   0x03   /* Echo payload back (simulated) */

/* Maximum payload bytes in a single executable packet */
#define EXEC_PAYLOAD_MAX 256

/* An executable packet sent in-network */
typedef struct {
    uint8_t  opcode;
    uint16_t payload_len;               /* actual bytes used in payload[] */
    uint8_t  payload[EXEC_PAYLOAD_MAX];
} exec_packet_t;

/* Execute an exec_packet_t.
 * Returns 0 on success, -1 on error. */
int execute_packet(const exec_packet_t *pkt);

/* Helper: build an OP_PRINT packet from a null-terminated string */
exec_packet_t exec_make_print(const char *msg);

/* Helper: build an OP_SUM packet from an array of int32_t values */
exec_packet_t exec_make_sum(const int32_t *values, size_t count);

#endif /* EXEC_H */
