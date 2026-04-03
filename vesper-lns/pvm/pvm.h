#ifndef PVM_H
#define PVM_H

#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Protocol Plugin ABI (vtable)
 *
 * Any code that wants to add a protocol registers a filled-in
 * protocol_ops_t with pvm_register().  The two built-ins (UDP, TCP) are
 * pre-registered by pvm_init() which is called lazily on first use.
 * ------------------------------------------------------------------------- */

#define PVM_MAX_PROTOCOLS  16

typedef struct {
    char  name[16];                         /* unique protocol identifier    */
    int (*send)(void *data, size_t len);    /* transmit bytes                */
    void (*on_tick)(void *ctx);             /* periodic background work      */
    void (*on_migrate)(const char *from,    /* called before a hot-swap      */
                       const char *to,
                       void *ctx);
} protocol_ops_t;

/* Register a protocol implementation.
 * Returns 0 on success, -1 if the registry is full or name already exists. */
int pvm_register(const protocol_ops_t *ops);

/* ---------------------------------------------------------------------------
 * Handle-based API (unchanged externally, extended internally)
 * ------------------------------------------------------------------------- */

/* Opaque protocol handle returned by pvm_load() */
typedef struct protocol_handle pvm_handle_t;

/* Load a named protocol.  Returns NULL if the name is unknown. */
pvm_handle_t *pvm_load(const char *name);

/* Execute a protocol send via a loaded handle.
 * Returns 0 on success, -1 on error. */
int pvm_execute(pvm_handle_t *handle, void *data, size_t len);

/* Atomically swap the protocol on a live handle without teardown.
 * on_migrate() of the outgoing protocol is called first (if set).
 * Returns 0 on success, -1 if name is unknown. */
int pvm_swap(pvm_handle_t *handle, const char *name);

/* Unload / release a protocol handle */
void pvm_unload(pvm_handle_t *handle);

/* Trigger on_tick on a live handle (no-op if not set) */
void pvm_tick(pvm_handle_t *handle, void *ctx);

#endif /* PVM_H */
