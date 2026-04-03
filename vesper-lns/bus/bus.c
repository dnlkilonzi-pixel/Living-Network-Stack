/*
 * bus/bus.c – Real UNIX-domain socketpair message bus
 *
 * Uses POSIX socketpair(AF_UNIX, SOCK_DGRAM) so every bus_send() call
 * produces a real kernel-buffered datagram that is independently received
 * by bus_recv() on the other end.  This is real IPC, not a simulation.
 */

/* Request POSIX.1-2008 symbols (select, socketpair, etc.) */
#define _POSIX_C_SOURCE 200809L

#include "bus.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

/* ---------------------------------------------------------------------------
 * Wire-format helpers: serialize / deserialize a bus_msg_t into a flat
 * byte buffer so the fields are transferred over the socket in a well-
 * defined layout, independent of host struct padding.
 * ------------------------------------------------------------------------- */

/* Serialized header is exactly BUS_HDR_SIZE bytes:
 *   [0 .. 3 ]  magic        (uint32_t, native endian)
 *   [4 .. 7 ]  msg_type     (uint32_t)
 *   [8 ..39 ]  src[32]
 *   [40..71 ]  dst[32]
 *   [72..75 ]  seq          (uint32_t)
 *   [76..79 ]  payload_len  (uint32_t)
 */

static void write_u32(uint8_t *buf, uint32_t v)
{
    buf[0] = (uint8_t)(v & 0xFFu);
    buf[1] = (uint8_t)((v >>  8) & 0xFFu);
    buf[2] = (uint8_t)((v >> 16) & 0xFFu);
    buf[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t read_u32(const uint8_t *buf)
{
    return  (uint32_t)buf[0]
         | ((uint32_t)buf[1] <<  8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

/* Serialize msg into wire_buf (must be >= BUS_HDR_SIZE + payload_len bytes).
 * Returns total bytes written. */
static int serialize_msg(const bus_msg_t *msg,
                         uint8_t *wire_buf, int wire_cap)
{
    int total = BUS_HDR_SIZE + (int)msg->payload_len;

    if (total > wire_cap) return -1;

    memset(wire_buf, 0, (size_t)BUS_HDR_SIZE);
    write_u32(wire_buf + 0,  msg->magic);
    write_u32(wire_buf + 4,  msg->msg_type);
    memcpy(wire_buf + 8,  msg->src, 32);
    memcpy(wire_buf + 40, msg->dst, 32);
    write_u32(wire_buf + 72, msg->seq);
    write_u32(wire_buf + 76, msg->payload_len);

    if (msg->payload_len > 0) {
        memcpy(wire_buf + BUS_HDR_SIZE, msg->payload, msg->payload_len);
    }

    return total;
}

/* Deserialize wire_buf into msg.  Returns 0 on success, -1 on error. */
static int deserialize_msg(const uint8_t *wire_buf, int wire_len,
                           bus_msg_t *msg)
{
    uint32_t plen;

    if (wire_len < BUS_HDR_SIZE) return -1;

    msg->magic       = read_u32(wire_buf + 0);
    msg->msg_type    = read_u32(wire_buf + 4);
    memcpy(msg->src, wire_buf + 8,  32);
    memcpy(msg->dst, wire_buf + 40, 32);
    msg->seq         = read_u32(wire_buf + 72);
    msg->payload_len = read_u32(wire_buf + 76);

    msg->src[31] = '\0';
    msg->dst[31] = '\0';

    if (msg->magic != BUS_MSG_MAGIC) {
        fprintf(stderr, "[BUS] Bad magic 0x%08x\n", msg->magic);
        return -1;
    }

    plen = msg->payload_len;
    if (plen > BUS_MAX_PAYLOAD) {
        fprintf(stderr, "[BUS] Payload too large: %u\n", plen);
        return -1;
    }
    if ((int)(BUS_HDR_SIZE + plen) > wire_len) return -1;

    memset(msg->payload, 0, BUS_MAX_PAYLOAD);
    if (plen > 0) {
        memcpy(msg->payload, wire_buf + BUS_HDR_SIZE, plen);
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * Internal lookup
 * ------------------------------------------------------------------------- */

static int find_node(const bus_t *bus, const char *label)
{
    int i;
    for (i = 0; i < bus->node_count; i++) {
        if (bus->nodes[i].valid &&
            strncmp(bus->nodes[i].label, label, 32) == 0) {
            return i;
        }
    }
    return -1;
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void bus_init(bus_t *bus)
{
    memset(bus, 0, sizeof(*bus));
    printf("[BUS] Message bus initialised\n");
}

void bus_shutdown(bus_t *bus)
{
    int i;

    for (i = 0; i < bus->node_count; i++) {
        if (!bus->nodes[i].valid) continue;
        close(bus->nodes[i].rx_fd);
        close(bus->nodes[i].tx_fd);
        bus->nodes[i].valid = 0;
    }
    bus->node_count = 0;
    printf("[BUS] Message bus shut down\n");
}

int bus_register(bus_t *bus, const char *label)
{
    int        idx;
    int        fds[2];
    bus_node_t *n;

    if (!label || bus->node_count >= BUS_MAX_NODES) {
        fprintf(stderr, "[BUS] Cannot register node\n");
        return -1;
    }

    /* socketpair: fds[0] is this node's rx end;
     *             fds[1] is the end remote nodes write to */
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) != 0) {
        perror("[BUS] socketpair");
        return -1;
    }

    idx           = bus->node_count++;
    n             = &bus->nodes[idx];
    memset(n, 0, sizeof(*n));
    strncpy(n->label, label, sizeof(n->label) - 1);
    n->rx_fd = fds[0];
    n->tx_fd = fds[1];
    n->valid = 1;

    printf("[BUS] Registered node \"%s\" (rx_fd=%d  tx_fd=%d)\n",
           n->label, n->rx_fd, n->tx_fd);
    return idx;
}

int bus_send(bus_t *bus, const char *src, const char *dst,
             uint32_t msg_type, const void *payload, size_t payload_len)
{
    int          dst_idx;
    bus_msg_t    msg;
    uint8_t      wire[BUS_HDR_SIZE + BUS_MAX_PAYLOAD];
    int          wire_len;

    if (payload_len > BUS_MAX_PAYLOAD) {
        fprintf(stderr, "[BUS] Payload too large (%zu > %d)\n",
                payload_len, BUS_MAX_PAYLOAD);
        return -1;
    }

    dst_idx = find_node(bus, dst);
    if (dst_idx < 0) {
        fprintf(stderr, "[BUS] Destination \"%s\" not registered\n", dst);
        return -1;
    }

    /* Build message */
    memset(&msg, 0, sizeof(msg));
    msg.magic    = BUS_MSG_MAGIC;
    msg.msg_type = msg_type;
    strncpy(msg.src, src ? src : "", sizeof(msg.src) - 1);
    strncpy(msg.dst, dst, sizeof(msg.dst) - 1);
    msg.seq      = bus->seq++;
    msg.payload_len = (uint32_t)payload_len;
    if (payload && payload_len > 0) {
        memcpy(msg.payload, payload, payload_len);
    }

    wire_len = serialize_msg(&msg, wire, (int)sizeof(wire));
    if (wire_len < 0) {
        fprintf(stderr, "[BUS] Serialization failed\n");
        return -1;
    }

    /* Write to the dst node's tx_fd (kernel buffers the datagram) */
    if (write(bus->nodes[dst_idx].tx_fd, wire, (size_t)wire_len)
            != wire_len) {
        perror("[BUS] write");
        return -1;
    }

    printf("[BUS] Sent: \"%s\" -> \"%s\"  type=0x%02x  "
           "seq=%u  payload=%u bytes\n",
           msg.src, msg.dst, msg.msg_type, msg.seq, msg.payload_len);
    return 0;
}

int bus_recv(bus_t *bus, const char *dst,
             bus_msg_t *msg_out, int timeout_ms)
{
    int          dst_idx;
    fd_set       rfds;
    struct timeval tv;
    uint8_t      wire[BUS_HDR_SIZE + BUS_MAX_PAYLOAD];
    int          n;
    int          rc;

    dst_idx = find_node(bus, dst);
    if (dst_idx < 0) {
        fprintf(stderr, "[BUS] Recv: node \"%s\" not registered\n", dst);
        return -1;
    }

    FD_ZERO(&rfds);
    FD_SET(bus->nodes[dst_idx].rx_fd, &rfds);
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    rc = select(bus->nodes[dst_idx].rx_fd + 1, &rfds, NULL, NULL, &tv);
    if (rc < 0) { perror("[BUS] select"); return -1; }
    if (rc == 0) return -1;   /* timeout */

    n = (int)read(bus->nodes[dst_idx].rx_fd, wire, sizeof(wire));
    if (n <= 0) {
        perror("[BUS] read");
        return -1;
    }

    return deserialize_msg(wire, n, msg_out);
}

void bus_msg_print(const bus_msg_t *msg)
{
    const char *type_str;

    switch (msg->msg_type) {
    case BUS_MSG_DATA:     type_str = "DATA";     break;
    case BUS_MSG_DECISION: type_str = "DECISION"; break;
    case BUS_MSG_METRICS:  type_str = "METRICS";  break;
    case BUS_MSG_EXEC:     type_str = "EXEC";     break;
    case BUS_MSG_PING:     type_str = "PING";     break;
    case BUS_MSG_PONG:     type_str = "PONG";     break;
    default:               type_str = "UNKNOWN";  break;
    }

    printf("[BUS] Message: magic=0x%08x  type=%s  "
           "from=\"%s\"  to=\"%s\"  seq=%u  payload=%u bytes\n",
           msg->magic, type_str, msg->src, msg->dst,
           msg->seq, msg->payload_len);

    if (msg->payload_len > 0 && msg->msg_type == BUS_MSG_DATA) {
        /* Print data payload as text (safe: force null terminator) */
        char txt[BUS_MAX_PAYLOAD + 1];
        size_t copy = msg->payload_len < BUS_MAX_PAYLOAD
                    ? msg->payload_len : BUS_MAX_PAYLOAD;
        memcpy(txt, msg->payload, copy);
        txt[copy] = '\0';
        printf("[BUS]   payload: \"%s\"\n", txt);
    }
}
