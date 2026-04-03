#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stddef.h>

/* Simulated UDP send.
 * In production this would open a datagram socket and call sendto().
 * Returns 0 on success, -1 on error. */
int udp_send(void *data, size_t len);

/* Simulated TCP send.
 * In production this would use a connected stream socket and call send().
 * Returns 0 on success, -1 on error. */
int tcp_send(void *data, size_t len);

#endif /* TRANSPORT_H */
