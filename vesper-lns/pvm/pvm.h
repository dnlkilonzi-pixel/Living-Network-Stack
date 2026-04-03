#ifndef PVM_H
#define PVM_H

#include <stddef.h>

/* Opaque protocol handle returned by pvm_load() */
typedef struct protocol_handle pvm_handle_t;

/* Load a named protocol.
 * Supported names: "udp", "tcp"
 * Returns NULL if the name is unknown. */
pvm_handle_t *pvm_load(const char *name);

/* Execute a protocol send via a loaded handle.
 * Returns 0 on success, -1 on error. */
int pvm_execute(pvm_handle_t *handle, void *data, size_t len);

/* Unload / release a protocol handle */
void pvm_unload(pvm_handle_t *handle);

#endif /* PVM_H */
