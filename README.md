# SEAKUTILS

A collection of low-level, performance-oriented utilities written in C, designed to be reused across my own systems projects.

This repository serves as a **personal C utility library**, focusing on:
- predictable performance
- minimal abstractions
- explicit ownership and memory behavior
- lock-free and low-overhead concurrency primitives

The goal is not to reimplement the C standard library, but to provide **well-scoped building blocks** commonly needed in systems programming, networking, and concurrent applications.

---

## Design Philosophy

- **Low-level by default**: thin abstractions, no hidden allocations
- **Performance-aware**: cache-line alignment, atomics, minimal contention
- **Explicit over implicit**: lifetimes, ownership, and threading models are clear
- **Composable primitives**: utilities are designed to work together
- **Portable C**: relies on C11 where possible, avoids platform-specific tricks unless justified

This library is primarily intended for **Linux-based systems programming**. (Windows portability is not a priority for me since
I don't use it, if you want it ask me for it or do it yourself)

---

## Header-only (stb-style):

Each module is implemented in a single .h file

- Users explicitly enable implementations via #define MODULE_IMPLEMENTATION
- a dedicated `README.md` per directory describing design decisions, guarantees, and usage

---

## Available Utilities

### Channels (`channels/`)

Lock-free communication primitives for inter-thread messaging.

- **SPSC (Single Producer / Single Consumer) channel**
- **SPMC (Single Producer / Multiple Consumers) channel**
- **MPSC (Multiple Producers / Single Consumer) channel**
- **MPMC (Multiple Producers / Multiple Consumers) channel**

Key characteristics:
- Benchmarks were run without batching
- **MPSC channel** on a **4-producer** / **1-consumer** setup:
    - Sustained **throughput**: ~**13 million messages per second**
    - Stable under long-running workloads (400M+ messages)
- lock-free algorithms using C11 atomics
- bounded ring buffers
- cache-line alignment to avoid false sharing
- predictable memory usage
- explicit close semantics

See `channels/README.md` for full documentation and usage examples.

---

### Thread Pool (`threadpool/`)

A minimal thread pool built on top of the provided channels.

Design goals:
- simple and explicit API
- no hidden futures/promises
- tasks represented as plain function pointers + context
- work distribution via lock-free channels
- clean shutdown semantics

    See `threadpool/README.md` for details.

---
### Job System (`job_system/`)

A **dependency-aware job scheduling system** built on top of the thread pool, designed for 
engine-level workloads and explicit execution phases.

This system focuses on predictable performance and low-overhead dependency management, 
rather than fine-grained task scheduling or future-based execution.

> **Job System Status**
>
>The Job System is functional and stable under heavy workloads.
>It uses a region-based arena for job handle allocation, enabling
>fast allocation and explicit reuse across execution phases.
>
>APIs and internal behavior may still evolve as the system matures.

- **Key Features**
    - **Job continuations (dependencies)**
        - Jobs can declare explicit dependencies via `job_then`
        - Continuations are scheduled automatically once prerequisites complete
    - **Parallel execution**
        - Independent jobs execute concurrently across worker threads
        - Dependency chains are enforced deterministically
    - **Deterministic behavior**
        - With a single worker thread, job execution order is guaranteed
    - **lock-free**
        - Built on MPMC channels and C11 atomics
        - No mutexes or condition variables
    - Arena-based allocation
        - Job handles are allocated from a pre-allocated region arena

- **Design Notes**
    - Jobs are **fire-and-forget**
        - No returned values, futures, or promises
    - Dependency chains (`job_then`) are **serialized locally by design**
        - Only jobs within the same dependency chain are sequential
        - Independent jobs remain fully parallel
    - Job scheduling and execution are CPU-bound
        - Busy-waiting is used internally
        - Best suited for compute-heavy workloads
    - The system is designed for **phase-based execution**
        - Typical usage includes frame updates, task graphs, or batch processing
    - Large numbers of jobs are supported
        - Theoretical capacity: ~4.2 million jobs
        - Practical tested limit: ~1 million jobs spawned at once
        - Actual limits depend on channel capacity and workload shape

**What This System Is Not**
- Not a fine-grained task runtime
- Not a future/promise-based executor
- Not designed for blocking or I/O-bound workloads
- Not intended to replace higher-level async frameworks

    See `job_system/README.md` for full API details, usage examples, and guarantees.

---
### Wait Group (`wait_group/`)

A **Wait Group** is a minimal synchronization primitive used to wait for a group
of independent operations to complete.

It provides an explicit phase barrier without spawning threads, allocating memory,
or interacting with schedulers.

The Wait Group is intentionally small and designed to **compose with the Job System**, but it's Scheduler-Agnostic.

- **Key Features**
    - Atomic counter-based synchronization
    - No memory allocation
    - No hidden threads
    - Lock-free and deterministic
    - Suitable for phase-based execution

- **Relationship with the Job System**
    - The Job System handles execution and dependencies
    - The Wait Group provides explicit synchronization points
    - Fan-out and phase barriers are built using Wait Groups
    - No futures, callbacks, or implicit joins

This separation keeps both systems small, predictable, and easy to reason about.

See `wait_group/README.md` for detailed usage examples and integration patterns.

---
### Yield / Cooperative Tasks (`yield/`)

> Note: This module only supports x86-64 processors.

A minimal **cooperative multitasking runtime** for C, enabling functions to **pause and resume** at defined points without threads. 
This module serves as a foundation for async-like workflows and higher-level primitives such as futures or coroutines.

- Key Features
    - Run multiple tasks in a round-robin fashion.
    - Explicit task yielding with `yield()`.
    - Lightweight context management with independent stacks.
    - Wait for all tasks to complete via `wait_for_tasks()`.

See `yield/README.md` for examples and more information.

---
### Arenas (`arenas/`)

Arena-style memory allocators for fast, predictable allocation patterns.

Available arenas:

- **String Arena**
  - Stores null-terminated strings contiguously
  - Indexed by offsets
  - No per-string free

- **Generic Arena**
  - Stores fixed-size elements of any type
  - Supports dynamic growth or fixed capacity
  - Single-threaded, cache-friendly

- **Region Arena (Epoch-based, Concurrent)**
  - Fixed-size regions with lazy allocation
  - Thread-safe allocation using C11 atomics
  - Explicit reuse via epoch-based resets
  - No per-element free, no ownership tracking
  - Designed for high-throughput, phase-based systems (e.g. job systems)

Key characteristics:
- Linear allocation with O(1) insertion
- Explicit lifetime management (reset, free)

See `arenas/README.md` for detailed API documentation, guarantees, and usage rules.
 
---
### Data Structures (`data_structures/`)

A collection of **fundamental, data structures** intended for internal
use by other modules in this repository.

These implementations are **not STL-like** and do not aim to provide
high-level safety, polymorphism, or developer-friendly abstractions.
They exist to serve as **explicit, predictable building blocks** in systems code.

Currently included:

- **Stack**
  - Fixed-capacity, array-backed stack
  - Explicit push/pop semantics

- **Linked List**
  - Doubly-linked list
  - No sentinels exposed at the API level
  - Includes both standard and constant-time comparison search variants

- **Deque**
  - Fixed-capacity deque implemented as a ring buffer
  - Logical (monotonic) head/tail indices with wrap-on-access
  - Push/pop from both ends in O(1)
  - No resizing, no hidden allocations

**Design Characteristics**
- Fixed capacity where applicable
- No hidden memory allocation beyond explicit construction (Except Linked List)
- Explicit failure modes (full / empty)

These data structures are intentionally **minimally documented**.
The code and headers are expected to be read directly by users of this module.

---
### Strings (`strings/`)

**ASCII-only** string utilities focused on explicit, byte-level manipulation.

This module avoids Unicode normalization and libc character classification 
helpers. All operations are deterministic and operate directly on raw bytes.

- **Key characteristics**:
    - ASCII-only by design
    - Bit-level character inspection
    - No hidden allocations
    - No locale or global state
    - Predictable and explicit behavior

This module is intended for systems code, tooling, parsers, and pipelines where
ASCII is sufficient and Unicode would introduce unnecessary complexity.

See `strings/README.md` for design usage details.

---
## Intended Use

This repository is intended to be:
- vendored directly into C projects
- used as a foundation for systems utilities, runtimes, or services
- extended incrementally as new reusable primitives emerge

It is **not** a drop-in replacement for higher-level frameworks or runtimes.

---

## Toolchain & Requirements

- C11-compatible compiler
- C11 atomics support
- POSIX threads (`pthread`) for multithreading utilities (Threadpool)
- Yield is only x86-64 for now
- Linux environment recommended

---

## Status

This is an actively evolving utility set.
APIs may change as designs are refined and new primitives are added.

Stability and correctness are prioritized over feature completeness.

---

## License

MIT (unless stated otherwise in a specific module)
