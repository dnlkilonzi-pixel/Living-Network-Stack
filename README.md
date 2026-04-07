<div align="center">

<h1> Living Network Stack</h1>

<p><em>A self-evolving, intent-driven programmable network runtime written in C99</em></p>

[![Build](https://img.shields.io/badge/build-passing-brightgreen?style=flat-square&logo=gnu)](#building)
[![Language](https://img.shields.io/badge/language-C99-blue?style=flat-square&logo=c)](#)
[![License](https://img.shields.io/badge/license-MIT-green?style=flat-square)](#license)
[![Author](https://img.shields.io/badge/author-Daniel%20Kimeu-orange?style=flat-square)](#author)

<p>
  <b>Designed and built entirely by <a href="#author">Daniel Kimeu</a></b><br>
  Protocol evolution · Semantic verification · Adversarial synthesis · Formal spec compilation
</p>

</div>

---

## Table of Contents

- [Overview](#overview)
- [Feature Highlights](#feature-highlights)
- [Architecture](#architecture)
- [Module Map](#module-map)
- [Phases of Development](#phases-of-development)
- [Getting Started](#getting-started)
- [SDK Quick-Start](#sdk-quick-start)
- [Demo Output](#demo-output)
- [API Reference](#api-reference)
- [Design Philosophy](#design-philosophy)
- [Author](#author)

---

## Overview

The **Living Network Stack (LNS)** is a research-grade, fully in-C99 programmable network runtime that treats protocols as first-class, evolvable objects. Rather than hard-coding transport choices, LNS resolves *intent* — a structured description of what the application needs — into a concrete protocol decision at runtime, then mutates, monitors, verifies, and synthetically evolves that decision under changing network conditions.

```
┌──────────────────────────────────────────────────────────────────────────┐
│  Application                                                             │
│    intent_t { LOW_LATENCY | HIGH_SECURITY | HIGH_THROUGHPUT }            │
│    intent_semantics_t { guarantee, max_latency_ms, max_loss, objective } │
└─────────────────────────────┬────────────────────────────────────────────┘
                              │  lns_forward()
                              
┌──────────────────────────────────────────────────────────────────────────┐
│  Living Network Stack Runtime                                            │
│  ┌──────────┐  ┌────────────┐  ┌──────┐  ┌────────────┐  ┌──────────┐  │
│  │ Intent   │→ │ Mutation   │→ │ PVM  │→ │  Network   │→ │ Replay / │  │
│  │ Engine   │  │ Engine     │  │      │  │  (routing) │  │ Observer │  │
│  └──────────┘  └────────────┘  └──────┘  └────────────┘  └──────────┘  │
│                                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────────┐  ┌─────────────────────┐ │
│  │ Semantic │  │ Synth    │  │ Spec Compiler│  │ Trust / Bus / RNG   │ │
│  │ Verifier │  │ (evolve) │  │ (compile)    │  │                     │ │
│  └──────────┘  └──────────┘  └──────────────┘  └─────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Feature Highlights

| Capability | Description |
|---|---|
|  **Intent-Driven Routing** | Resolve `LOW_LATENCY`, `HIGH_SECURITY`, `HIGH_THROUGHPUT` flags into UDP/TCP decisions automatically |
|  **Adaptive Mutation** | Protocol decisions mutate in-place based on observed latency, loss, and bandwidth |
|  **Protocol Config Evolution** | `proto_config_t` parameters (window, RTO, cwnd, packet size) evolve under live conditions |
|  **Backpressure / Stress Model** | Accurate queue-depth model raises latency and loss proportionally to congestion |
|  **Protocol Virtual Machine** | Hot-swap protocol implementations at runtime via vtable registry (PVM) |
|  **Deterministic Replay** | Every hop recorded; re-execution from seed proves bit-identical determinism |
|  **Observer + Query Language** | Real-time dashboard + predicate-based query engine over the replay log |
|  **Real IPC Message Bus** | UNIX `socketpair` binary-serialized message bus for inter-node communication |
|  **Trust & Capability Routing** | Per-node trust scores gate capability access during routing decisions |
|  **Vector Clocks** | Causal ordering of all events across distributed nodes |
|  **Semantic Verification** | Check every hop against declared delivery guarantees, latency budgets, and loss budgets |
|  **Adversarial Synthesis** | Hillclimber evolves `proto_config_t` under hostile stress to satisfy semantic constraints |
|  **Formal Spec Compiler** | Compile `intent_t + intent_semantics_t` into an executable constraint program with runtime assertions |

---

## Architecture

```
vesper-lns/
├── core/               # lns_send() single-node entry point
├── intent_engine/      # intent_t → decision_t resolution
├── mutation_engine/    # Adaptive mutation + stress model + proto_config evolution
├── pvm/                # Protocol Virtual Machine – hot-swap vtable
├── execution_engine/   # Bytecode-style packet execution (OP_PRINT, OP_SUM, ...)
├── identity_layer/     # Node identity and capability checks
├── transport/          # UDP / TCP simulated transport implementations
├── network/            # Multi-node graph, shortest-path routing, decision propagation
├── trust_layer/        # Per-node trust scores and capability routing
├── bus/                # UNIX socketpair IPC + binary message serialization
├── replay/             # Deterministic event log, NDJSON export, re-execution
├── observer/           # System dashboard + observability query language
├── rng/                # Centralized xorshift64 PRNG (deterministic replay)
├── semantic/           # Semantic verification layer (Phase 6)
├── synth/              # Adversarial protocol synthesiser (Phase 7)
├── spec_compiler/      # Formal spec-to-constraint compiler (Phase 7)
├── sdk/                # Public SDK façade (lns_sdk.h / lns_sdk.c + examples)
└── demo/               # Full demonstration program (main.c)
```

---

## Module Map

### Core Data Types

```c
// What the application declares
intent_t           { flags, priority, ttl_ms }
intent_semantics_t { guarantee, max_latency_ms, max_loss, objective }

// What the runtime decides
decision_t         { use_udp, use_encryption, retry_count, exec_packet }

// What the network observes
net_metrics_t      { latency_ms, packet_loss, bandwidth_mbps }

// How parameters evolve
proto_config_t     { window_size, retransmit_delay_ms, packet_size, congestion_window }

// Adversarial stress state
stress_state_t     { queue_bytes, jitter_ms }
```

### Intent Flags

```c
INTENT_LOW_LATENCY      (1 << 0)   // prefer UDP, minimize RTT
INTENT_HIGH_THROUGHPUT  (1 << 1)   // large windows, jumbo frames
INTENT_HIGH_SECURITY    (1 << 2)   // require encryption, force TCP
```

### Delivery Guarantees

```c
GUARANTEE_BEST_EFFORT    // fire-and-forget
GUARANTEE_AT_LEAST_ONCE  // no drops; duplicates tolerated
GUARANTEE_EXACTLY_ONCE   // TCP-grade: no drops AND no duplicates
```

### Optimisation Objectives

```c
OBJECTIVE_MINIMIZE_LATENCY     // prefer lowest observed latency
OBJECTIVE_MINIMIZE_LOSS        // prefer lowest packet-loss path
OBJECTIVE_MAXIMIZE_THROUGHPUT  // prefer highest effective bandwidth
```

---

## Phases of Development

### Phase 1 — Core Runtime
Single-node intent resolution, mutation engine, Protocol VM, identity-based routing, execution engine (bytecode ops).

### Phase 2 — Distributed Network
Multi-node graph with shortest-path routing, decision propagation across nodes, trust & capability layer, distributed computation ops (`OP_SUM_PART`, `OP_AGGREGATE`), live `proto_config_t` evolution.

### Phase 3 — Observability & IPC
Deterministic event replay log, global observer dashboard, real UNIX `socketpair` message bus with binary serialization. Network layer gains `network_set_replay()` and `network_set_observer()` hooks.

### Phase 4 — SDK
`lns_session_t` public façade (`lns_sdk.h` / `lns_sdk.c`), `make lib` builds `liblns.a`, SDK example programs, convergence timing in observer, NDJSON replay export (`replay_dump_file()`).

### Phase 5 — Reliability & Causality
Centralized xorshift64 PRNG (`rng/`) for bit-identical determinism, `stress_state_t` backpressure model in mutation engine, vector clocks in every `replay_event_t`, PVM vtable registry with hot-swap, `observer_query()` predicate query layer.

### Phase 6 — Semantic Verification
`intent_semantics_t` formal constraint declarations, `semantic_verify_session()` checks every hop against latency/loss/guarantee budgets, `semantic_compare_protocols()` ranks TCP vs UDP under identical stress, violation reports and comparison tables. SDK gains `lns_semantic_verify()` + `lns_semantic_compare()`.

### Phase 7 — Adversarial Synthesis & Formal Spec Compilation *(current)*

####  Adversarial Protocol Synthesiser (`synth/`)
A fixed-population hillclimber that evolves `proto_config_t` parameters under **adversarial** (worst-case) stress conditions to discover the configuration that best satisfies the declared `intent_semantics_t` constraints.

- **8 mutants per generation**, each field perturbed ±15–30%
- Inner scoring loop is **pure** — mirrors `stress_update()` arithmetic with zero I/O
- Fitness function penalises constraint violations, rewards declared objective direction
- Converges early after 4 non-improving consecutive generations (up to 32 max)

```c
synth_config_t cfg = synth_config_default();
cfg.stress.queue_bytes  = 800000.0f;       // pre-fill queue (hostile!)
cfg.semantics.guarantee = GUARANTEE_AT_LEAST_ONCE;
cfg.semantics.max_loss  = 0.05f;
cfg.semantics.objective = OBJECTIVE_MINIMIZE_LATENCY;

synth_result_t result;
lns_synth_run(&cfg, &result);
synth_print_result(&result);
```

```
============================================================
  [SYNTH] Adversarial Protocol Synthesis Result
============================================================
  Generations run : 12 (converged early)
  Best score      : 847.23 / 1000.00
  Best found gen  : 8
  Violates        : NO
  -------------------------------------------------------
  Winning proto_config_t:
  [PROTO_CFG] window=98304 bytes  rto=142 ms  packet_size=1187 bytes  cwnd=1
  -------------------------------------------------------
  Score history:
  621 → 698 → 712 → 756 → 780 → 801 → 820 → 847 → 847 → 847 → 847 → 847
============================================================
```

####  Formal Spec Compiler (`spec_compiler/`)
Compiles an `(intent_t, intent_semantics_t)` pair into a runnable `spec_program_t` — a flat array of typed constraint rules that make intent declarations **executable** at runtime.

| Source | Compiled Rule |
|---|---|
| `INTENT_LOW_LATENCY` | `latency_ms < 50.00 ms` |
| `INTENT_HIGH_THROUGHPUT` | `bandwidth_mbps > 10.00 Mbps` |
| `INTENT_HIGH_SECURITY` | `use_encryption == 1` |
| `intent.ttl_ms > 0` | `latency_ms < ttl_ms` |
| `semantics.max_latency_ms` | `latency_ms < budget` |
| `semantics.max_loss` | `packet_loss < budget` |
| `semantics.guarantee ≥ AT_LEAST_ONCE` | `use_udp == 0` (TCP required) |
| `semantics.objective` | advisory performance rule |

```c
spec_program_t prog;
lns_spec_compile(intent, semantics, &prog);
spec_print_program(&prog);                    // print symbolic rules

int violations = lns_spec_check_session(session, &prog);
spec_print_violations(&prog);                 // per-rule violation counts
```

```
============================================================
  [SPEC] Compiled Constraint Program  (5 rules)
============================================================
  Source intent flags: 0x01  (LOW_LATENCY )
  Source guarantee : 1  max_latency=80.0 ms  max_loss=0.0500
  ------------------------------------------------------------
  No.  Name                            Rule
  ------------------------------------------------------------
  [00] low_latency_flag                latency_ms < 50.00  [from INTENT_LOW_LATENCY]
  [01] ttl_latency_budget              latency_ms < 500.00  [from intent.ttl_ms]
  [02] latency_budget                  latency_ms < 80.00  [from semantics.max_latency_ms]
  [03] loss_budget                     packet_loss < 0.0500  [from semantics.max_loss]
  [04] delivery_guarantee_tcp          use_udp == 0  [delivery guarantee: AT_LEAST_ONCE]
============================================================
```

---

## Getting Started

### Prerequisites

```bash
gcc (any version supporting -std=c99)
make
```

### Build

```bash
# Clone
git clone https://github.com/dnlkilonzi-pixel/Living-Network-Stack.git
cd Living-Network-Stack/vesper-lns

# Build demo binary
make

# Build static library (liblns.a)
make lib

# Build SDK example programs
make examples

# Build + run demo
make run

# Clean all artifacts
make clean
```

### Build Output

```
gcc -std=c99 -Wall -Wextra -Wpedantic -g ...
Build successful: lns_demo

gcc ... -c -o synth/synth.o synth/synth.c
gcc ... -c -o spec_compiler/spec_compiler.o spec_compiler/spec_compiler.c
ar rcs liblns.a core/lns.o ... synth/synth.o spec_compiler/spec_compiler.o sdk/lns_sdk.o
Library built: liblns.a

Built: sdk/examples/example_basic
Built: sdk/examples/example_replay
```

---

## SDK Quick-Start

Include only `sdk/lns_sdk.h` and link against `liblns.a`:

```c
#include "lns_sdk.h"

int main(void) {
    /* 1 — Create session */
    lns_session_t *s = lns_session_create(42);

    /* 2 — Build topology */
    lns_node_add(s, "edge-A",  45.0f, 0.02f,  70.0f);
    lns_node_add(s, "relay-B", 30.0f, 0.05f,  50.0f);
    lns_node_add(s, "core-C",  10.0f, 0.005f, 200.0f);
    lns_node_connect(s, "edge-A",  "relay-B");
    lns_node_connect(s, "relay-B", "core-C");

    /* 3 — Send with intent */
    intent_t intent = { INTENT_LOW_LATENCY, 10, 500 };
    char payload[] = "fintech-tx: amount=9999.00";
    lns_forward(s, "edge-A", "core-C", intent, payload, sizeof(payload)-1);

    /* 4 — Observe */
    lns_observe(s);                          // dashboard
    lns_replay_dump(s, stdout);              // human-readable trace
    lns_replay_verify(s);                    // determinism assertion

    /* 5 — Semantic verification */
    intent_semantics_t sem = {
        .guarantee      = GUARANTEE_AT_LEAST_ONCE,
        .max_latency_ms = 100.0f,
        .max_loss       = 0.05f,
        .objective      = OBJECTIVE_MINIMIZE_LATENCY
    };
    semantic_violation_t violations[32];
    int count = lns_semantic_verify(s, &sem, violations, 32);
    semantic_print_violations(violations, count);

    /* 6 — Formal spec check */
    spec_program_t prog;
    lns_spec_compile(intent, sem, &prog);
    spec_print_program(&prog);
    lns_spec_check_session(s, &prog);
    spec_print_violations(&prog);

    /* 7 — Adversarial synthesis */
    synth_config_t cfg = synth_config_default();
    cfg.semantics = sem;
    synth_result_t result;
    lns_synth_run(&cfg, &result);
    synth_print_result(&result);

    /* 8 — Teardown */
    lns_session_destroy(s);
    return 0;
}
```

Compile:
```bash
gcc -std=c99 -I../vesper-lns -o my_app my_app.c vesper-lns/liblns.a
```

---

## Demo Output

Running `make run` exercises all subsystems:

```
============================================================
  LNS — Phase 1: Intent Resolution & Mutation
============================================================

[INTENT] flags=0x01 (LOW_LATENCY) priority=10 ttl=500ms
[DECISION] use_udp=1 encrypt=0 retries=0 exec_packet=0
[METRICS] latency=147.0 ms  loss=17.0%  bandwidth=42.0 Mbps
[MUTATION] High latency detected -> switching to UDP
[MUTATION] High packet loss (17.0%) -> increasing retries from 0 to 2

============================================================
  LNS — Phase 3: Replay & Observer
============================================================

[REPLAY] seq=0001 node=edge-A  HOP 1/3  UDP  lat=45.0ms loss=2.0%
[REPLAY] seq=0002 node=relay-B HOP 2/3  UDP  lat=30.0ms loss=5.0%
[REPLAY] seq=0003 node=core-C  HOP 3/3  TCP  lat=10.0ms loss=0.5%

============================================================
  [OBSERVER] System Dashboard
============================================================
  Total forwards    : 4        Successful: 4    Failed: 0
  Total hops        : 12       UDP: 9           TCP: 3
  Protocol switches : 2
  Avg latency       : 28.3 ms
  Avg packet loss   : 2.4 %
  Convergence events: 1        Avg time: 14 ms
============================================================

============================================================
  [SYNTH] Adversarial Protocol Synthesis Result
============================================================
  Generations run : 8 (converged early)
  Best score      : 912.40 / 1000.00
  Violates        : NO
  Winning proto_config_t:
  [PROTO_CFG] window=87040 bytes  rto=165 ms  packet_size=1289 bytes  cwnd=1
============================================================
```

---

## API Reference

### Session Lifecycle

| Function | Description |
|---|---|
| `lns_session_create(seed)` | Create a seeded session (heap-allocated) |
| `lns_session_destroy(s)` | Tear down and free session |

### Topology

| Function | Description |
|---|---|
| `lns_node_add(s, label, lat, loss, bw)` | Add a node with baseline metrics |
| `lns_node_connect(s, from, to)` | Add a directed edge |

### Forwarding

| Function | Description |
|---|---|
| `lns_forward(s, src, dst, intent, data, len)` | Intent-driven send across topology |

### Observability

| Function | Description |
|---|---|
| `lns_observe(s)` | Print observer dashboard |
| `lns_observe_snapshot(s)` | Return `obs_stats_t` snapshot |
| `lns_replay_dump(s, fp)` | Human-readable trace to file |
| `lns_replay_verify(s)` | Re-execute and assert determinism |
| `lns_replay_save(s, path)` | NDJSON trace to disk |

### Semantic Verification

| Function | Description |
|---|---|
| `lns_semantic_verify(s, sem, out, max)` | Check all hops against declared constraints |
| `lns_semantic_compare(s, sem, result)` | Rank TCP vs UDP under declared objective |
| `semantic_print_violations(v, n)` | Print violation report |
| `semantic_print_comparison(r)` | Print side-by-side comparison table |

### Adversarial Synthesis

| Function | Description |
|---|---|
| `lns_synth_run(cfg, result)` | Evolve `proto_config_t` under stress |
| `synth_config_default()` | Safe default synthesis config |
| `synth_print_result(r)` | Print winning config + score history |

### Formal Spec Compiler

| Function | Description |
|---|---|
| `lns_spec_compile(intent, sem, prog)` | Compile intent pair → constraint program |
| `lns_spec_check_session(s, prog)` | Sweep program over full replay log |
| `spec_check(prog, metrics, decision)` | Check one hop inline |
| `spec_assert_hop(prog, metrics, dec, node)` | Inline assertion (stderr on fail) |
| `spec_print_program(prog)` | Print symbolic rules |
| `spec_print_violations(prog)` | Print cumulative violation counts |

---

## Design Philosophy

1. **Everything is deterministic.** A single seed uniquely determines every random draw in the system via the centralized xorshift64 PRNG. Given the same seed, `make run` produces identical output byte-for-byte, every time.

2. **Protocols are first-class objects.** The PVM registry allows any code to register a protocol implementation (`protocol_ops_t` vtable) and hot-swap it onto a live handle without teardown.

3. **Intent precedes mechanism.** Applications declare *what they need* (`intent_t`), not *which protocol to use*. The runtime discovers the right protocol, mutates it under stress, and verifies the outcome.

4. **Observability is built in.** Every routing decision, every metric observation, every causal relationship between events is recorded in a structured replay log with vector clocks. Nothing is invisible.

5. **Constraints are executable.** `intent_semantics_t` is not just documentation — the spec compiler turns it into a program that runs assertions against every hop and accumulates evidence of violations.

6. **Adversarial by default.** The synthesis engine assumes hostile conditions (saturated queues, high jitter) when searching for optimal configurations, so the winning config is robust to real-world stress.

---

## Author

<div align="center">

```
╔══════════════════════════════════════════════════╗
║                                                  ║
║   Living Network Stack                           ║
║   Designed, architected, and built by            ║
║                                                  ║
║          Daniel Kimeu                            ║
║                                                  ║
║   All concepts, modules, algorithms,             ║
║   and implementations in this repository         ║
║   are the original work of Daniel Kimeu.         ║
║                                                  ║
╚══════════════════════════════════════════════════╝
```

**Daniel Kimeu** is the sole author of the Living Network Stack — from the initial intent engine through the formal spec compiler. Every protocol, every algorithm, every design decision traces back to his vision of a programmable, self-evolving network runtime.

GitHub · [@dnlkilonzi-pixel](https://github.com/dnlkilonzi-pixel)

</div>

---

## License

MIT © Daniel Kimeu