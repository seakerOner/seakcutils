# Manifesto

## SEAKUTILS — A Statement of Intent

I believe that developers **should be in control**.

Software should not babysit the developer, nor should it hide its behavior behind opaque abstractions 
that create the illusion of control. When systems become obscure, understanding degrades and once 
understanding is lost, software quality follows.

I do not believe in building identity around frameworks or libraries.
The idea of being an “X Developer” is a fallacy.

We use tools.
**Tools do not use us**.

Frameworks and runtimes are temporary.
Understanding is not.

---

## Why This Project Exists

This repository exists to provide foundational building blocks; not opinions, not workflows, not imposed architectures.

The utilities in this project are designed to be:

- simple
- transparent
- explicit
- modular

Each component is meant to be understood fully by the developer using it.
Nothing relies on hidden threads, implicit scheduling, or invisible ownership.

If you use these utilities, you should be able to reason about what your program is doing at all times.

---

## Explicit Over Implicit

This project values **explicit control over convenience**.

- Execution is explicit
- Scheduling is explicit
- Ownership is explicit
- Synchronization is explicit

If something runs, it is because **you scheduled it**.
If something waits, it is because **you asked it to wait**.

---

## Primitives, Not Frameworks

SEAKUTILS does not aim to be a framework.

It provides small, composable primitives that can be layered to build higher-level systems; if and only if the developer chooses to do so.

Examples include:
- Channels for message passing
- Thread pools for execution
- Job systems for dependency-aware scheduling
- Wait groups for phase synchronization

Higher-level abstractions (such as async executors or futures) may exist, but they are built on top of these primitives, not fused into them.

You pay only for what you use. 

---

## No Abstraction Worshiping

This project avoids:
- over-generalization
- feature-driven design
- abstraction for its own sake

Every abstraction in this repository must justify itself in terms of:
- clarity
- performance
- composability
- long-term maintainability

---

## Long-Term Thinking

This library is written with the assumption that:
- frameworks come and go
- trends fade

The goal is not to chase modernity, but to build durable systems that remain understandable years later.

C is chosen deliberately:

- for its stability
- for its transparency
- for its lack of imposed models

This is software meant to age well, or I will try.

---

## Who This is For

It's primarily for myself but this project is for developers who:

- value understanding over convenience
- want to build systems, not glue frameworks together
- prefer explicit control over implicit magic

If you want a framework that decides how your program should be structured, this is not it.

---

## Final Note

SEAKUTILS is not about doing more.
It is about doing less, but knowing exactly why.

You are expected to think.
You are expected to compose.
You are expected to remain in control.

That is the point.

> Copyright 2025 Seaker <seakerone@proton.me>


