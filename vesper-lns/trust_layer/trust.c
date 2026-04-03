#include "trust.h"
#include <stdio.h>
#include <string.h>

#define TRUST_REGISTRY_MAX 16

static node_profile_t registry[TRUST_REGISTRY_MAX];
static int            registry_count = 0;

void trust_registry_init(void)
{
    registry_count = 0;
    printf("[TRUST] Registry initialised\n");
}

void trust_registry_add(node_profile_t profile)
{
    if (registry_count >= TRUST_REGISTRY_MAX) {
        fprintf(stderr, "[TRUST] Registry full (max %d)\n",
                TRUST_REGISTRY_MAX);
        return;
    }
    registry[registry_count++] = profile;
    printf("[TRUST] Registered: \"%s\"  trust=%d  caps=0x%02x\n",
           profile.label, profile.trust_score, profile.capabilities);
}

const node_profile_t *trust_lookup(node_id_t id)
{
    int i;
    for (i = 0; i < registry_count; i++) {
        if (memcmp(registry[i].id.id, id.id, NODE_ID_LEN) == 0) {
            return &registry[i];
        }
    }
    return NULL;
}

int trust_check(node_id_t id, int min_trust)
{
    const node_profile_t *p = trust_lookup(id);
    int ok;

    if (!p) {
        printf("[TRUST] Node not found in registry -> treated as untrusted\n");
        return 0;
    }

    ok = (p->trust_score >= min_trust);
    printf("[TRUST] \"%s\"  trust=%d  min_required=%d  -> %s\n",
           p->label, p->trust_score, min_trust,
           ok ? "TRUSTED" : "UNTRUSTED");
    return ok;
}

int trust_route_select(const node_id_t *candidates, int count,
                       int required_cap, int min_trust,
                       node_id_t *selected)
{
    int best_idx   = -1;
    int best_trust = -1;
    int i;

    for (i = 0; i < count; i++) {
        const node_profile_t *p = trust_lookup(candidates[i]);

        if (!p) {
            printf("[TRUST] Candidate %d: not in registry, skipping\n", i);
            continue;
        }

        if (p->trust_score < min_trust) {
            printf("[TRUST] Skipping \"%s\" (trust=%d < min=%d)\n",
                   p->label, p->trust_score, min_trust);
            continue;
        }

        if (required_cap && !(p->capabilities & required_cap)) {
            printf("[TRUST] Skipping \"%s\" (missing capability 0x%02x)\n",
                   p->label, required_cap);
            continue;
        }

        if (p->trust_score > best_trust) {
            best_trust = p->trust_score;
            best_idx   = i;
        }
    }

    if (best_idx >= 0) {
        const node_profile_t *p = trust_lookup(candidates[best_idx]);
        *selected = candidates[best_idx];
        printf("[TRUST] Selected \"%s\" (trust=%d  caps=0x%02x)\n",
               p->label, p->trust_score, p->capabilities);
    } else {
        printf("[TRUST] No qualifying candidate found "
               "(min_trust=%d, required_cap=0x%02x)\n",
               min_trust, required_cap);
    }

    return best_idx;
}

void trust_update(node_id_t id, int delta)
{
    int i;

    for (i = 0; i < registry_count; i++) {
        if (memcmp(registry[i].id.id, id.id, NODE_ID_LEN) == 0) {
            int old                = registry[i].trust_score;
            registry[i].trust_score += delta;
            if (registry[i].trust_score < 0)   registry[i].trust_score = 0;
            if (registry[i].trust_score > 100)  registry[i].trust_score = 100;
            printf("[TRUST] Updated \"%s\": trust %d -> %d\n",
                   registry[i].label, old, registry[i].trust_score);
            return;
        }
    }

    fprintf(stderr, "[TRUST] trust_update: node not found in registry\n");
}

void trust_print(const node_profile_t *p)
{
    if (!p) return;
    printf("[TRUST] \"%s\"  trust=%d  caps=[%s%s%s%s]\n",
           p->label,
           p->trust_score,
           (p->capabilities & CAP_COMPUTE) ? "COMPUTE "  : "",
           (p->capabilities & CAP_RELAY)   ? "RELAY "    : "",
           (p->capabilities & CAP_STORAGE) ? "STORAGE "  : "",
           (p->capabilities & CAP_CRYPTO)  ? "CRYPTO"    : "");
}

void trust_registry_print(void)
{
    int i;
    printf("[TRUST] === Registry (%d nodes) ===\n", registry_count);
    for (i = 0; i < registry_count; i++) {
        trust_print(&registry[i]);
    }
}
