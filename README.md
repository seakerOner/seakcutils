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

This library is primarily intended for **Linux-based systems programming**, but portability is considered whenever feasible.

---

## Repository Structure

Each utility lives in its own directory and includes:
- implementation (`.c`)
- public API (`.h`)
- a dedicated `README.md` describing design decisions, guarantees, and usage

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

A **dependency-aware job scheduling** system built on top of the thread pool.

> **Job System Status**
>
> The Job System currently relies on arena-based allocation for job handles.
> A low-level, epoch-based Region Arena has been implemented to support
> stable handles and reuse across execution phases.
>
> Full integration of the Region Arena into the Job System is still in progress.
> Until this integration is complete, the Job System should be considered
> **experimental** and not yet suitable for production workloads.

- Key features:
    - Job continuations: schedule dependent jobs automatically when prerequisites complete
    - Parallel execution: multiple worker threads execute independent jobs concurrently
    - Deterministic single-threaded execution: job order is guaranteed when using 1 worker thread
    - Lock-free and low-overhead: leverages MPMC channels and atomic counters
    - No dynamic allocation during execution: job handles allocated in a pre-allocated arena
- Design notes:
    - Jobs are fire-and-forget, with no returned result
    - Continuation dependencies are enforced
    - Multi-threaded execution may run independent jobs in any order, while still respecting dependencies
    - Users can combine jobs with existing channels for complex pipelines

    See `job_system/README.md` for full API details, usage examples, and guarantees.

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

- **Region Arena (Epoch-based, Concurrent, Low-level)**
  - Fixed-size regions with lazy allocation
  - Thread-safe allocation using C11 atomics
  - Explicit reuse via epoch-based resets
  - No per-element free, no ownership tracking
  - Designed for high-throughput, phase-based systems (e.g. job systems)

Key characteristics:
- Linear allocation with amortized O(1) insertion
- Explicit lifetime management (reset, free)

See `arenas/README.md` for detailed API documentation, guarantees, and usage rules.
 
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
- Linux environment recommended

---

## Status

This is an actively evolving utility set.
APIs may change as designs are refined and new primitives are added.

Stability and correctness are prioritized over feature completeness.

---

## License

MIT (unless stated otherwise in a specific module)
