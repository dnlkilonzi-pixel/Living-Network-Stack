#ifndef BUS_H
#define BUS_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Real IPC message bus backed by UNIX-domain socketpairs.
 *
 * Each registered node gets a kernel-level socketpair:
 *   node->rx_fd  – the end this node reads from
 *   node->tx_fd  – the end other nodes write to
 *
 * Messages are serialized as a fixed 80-byte header followed by a variable
 * payload.  Header layout (all native-endian uint32_t):
 *   [0]  magic       (BUS_MSG_MAGIC)
 *   [4]  msg_type    (BUS_MSG_*)
 *   [8]  src[32]     source node label
 *   [40] dst[32]     destination node label
 *   [72] seq         global message counter
 *   [76] payload_len bytes that follow this header
 * Total header size: 80 bytes.
 * ------------------------------------------------------------------------- */

#define BUS_MAX_NODES     8
#define BUS_MAX_PAYLOAD   512
#define BUS_HDR_SIZE      80   /* 4+4+32+32+4+4 */

#define BUS_MSG_MAGIC     0x4C4E5342u  /* 'L','N','S','B' */

/* Message types */
#define BUS_MSG_DATA      0x01u   /* raw data payload */
#define BUS_MSG_DECISION  0x02u   /* serialized decision_packet_t */
#define BUS_MSG_METRICS   0x03u   /* serialized net_metrics_t */
#define BUS_MSG_EXEC      0x04u   /* execution opcode packet */
#define BUS_MSG_PING      0x05u   /* liveness probe */
#define BUS_MSG_PONG      0x06u   /* liveness reply */

/* Parsed message (after deserialization) */
typedef struct {
    uint32_t magic;
    uint32_t msg_type;
    char     src[32];
    char     dst[32];
    uint32_t seq;
    uint32_t payload_len;
    uint8_t  payload[BUS_MAX_PAYLOAD];
} bus_msg_t;

/* One registered node endpoint */
typedef struct {
    char label[32];
    int  rx_fd;   /* this node reads from here */
    int  tx_fd;   /* remote nodes write to here */
    int  valid;
} bus_node_t;

/* The message bus */
typedef struct {
    bus_node_t nodes[BUS_MAX_NODES];
    int        node_count;
    uint32_t   seq;           /* global sequence counter */
} bus_t;

/* Initialise the bus (zero state, open no sockets) */
void bus_init(bus_t *bus);

/* Close all sockets and reset the bus */
void bus_shutdown(bus_t *bus);

/* Register a new node; returns its index or -1 on failure.
 * Internally creates a socketpair for this node. */
int bus_register(bus_t *bus, const char *label);

/* Send a message from src to dst (by label).
 * Returns 0 on success, -1 on error. */
int bus_send(bus_t *bus, const char *src, const char *dst,
             uint32_t msg_type, const void *payload, size_t payload_len);

/* Receive a message addressed to dst (by label).
 * timeout_ms = 0 → non-blocking; > 0 → block up to N ms.
 * Returns 0 on success, -1 on timeout / error. */
int bus_recv(bus_t *bus, const char *dst,
             bus_msg_t *msg_out, int timeout_ms);

/* Pretty-print a received message header */
void bus_msg_print(const bus_msg_t *msg);

#endif /* BUS_H */
