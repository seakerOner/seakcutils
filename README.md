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

Key characteristics:
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
