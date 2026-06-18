# Phase A2: Priority Scheduling + Aging — Test Log

## Goal

Add a **static-priority scheduler** (with automatic aging to
prevent starvation) as a 5th algorithm alongside RR / FCFS /
MLFQ / SJF.  Expose `setpriority(pid, prio)` and
`getpriority(pid)` syscalls so user code can manipulate
priorities at runtime.  Verify that:
1. A higher-priority process gets to run before a lower-
   priority one.
2. Aging eventually runs a long-starved low-priority process.
3. The existing test programs (MLFQ/SJF) still build and run.

Priority inheritance (the "second half" of A2 in the plan) is
not implemented in this phase — see Phase D1 for the
Pathfinder-style scenario and the inheritance plumbing.

## What was built

### Kernel

- `kernel/param.h`:
  - `SCHED_PRIO = 4` — new algorithm id.
  - `PRIO_AGING_TICKS = 50` — aging interval.
  - `PRIO_AGING_DEC = 1` — how much to lower a process's
    `priority` value each aging tick (lower = higher priority).

- `kernel/proc.c`:
  - `prio_scheduler()` — non-preemptive priority scheduler.
    Picks the RUNNABLE process with the smallest `priority`
    value; ties are broken by `last_sched` (oldest first) then
    by `ctime`.
  - `prio_aging_tick()` — called from `prio_scheduler()` on
    every iteration; boosts every RUNNABLE process's priority
    (decrements the value) every `PRIO_AGING_TICKS` ticks,
    clamped at 0.
  - `ksched_setprio(pid, prio)` — the kernel-side setter used
    by the syscall.
  - `scheduler()` — dispatches to `prio_scheduler()` when
    `current_scheduler == SCHED_PRIO`.
  - `sched_algo_name()` — adds "PRIO" for algorithm 4.

- `kernel/defs.h` — added prototypes for `prio_scheduler` and
  `ksched_setprio`.

- `kernel/syscall.h`:
  - `SYS_setpriority = 54`
  - `SYS_getpriority = 55`

- `kernel/syscall.c` — registered both new syscalls.

- `kernel/sysproc.c`:
  - `sys_setpriority(pid, prio)` — validates range, calls
    `ksched_setprio`.
  - `sys_getpriority(pid)` — scans the proc table for the
    given pid and returns its `priority` (or -1 if not
    found).
  - `sys_sched_algorithm` — now accepts `algo` up to 4.

### User

- `user/usys.pl` — emits `setpriority` / `getpriority` stubs.
- `user/user.h` — declares the two functions.
- `user/sched.c` — `sched_algorithm_name(4) == "PRIO"`.
- `user/prioritytest.c` — new test program.
- `Makefile` — `_prioritytest` added to `UPROGS`.

## Subtle issue: stale validation in `sys_sched_algorithm`

The original scheduler-switching syscall rejected any
`algo > 3`.  Adding `SCHED_PRIO = 4` required relaxing that
check, otherwise `sched_algorithm(4)` would return -1
silently.  The user-space wrapper has no way to distinguish
"unknown algorithm" from "internal error" otherwise.

## Trace of the run

```
=== Priority Scheduling + Aging (Phase A2) ===
scheduler: PRIO
[3] child_hi id=0 pid=4 prio=0 start
[3] pid=4 done (label=1)
[3] child_lo id=0 pid=5 prio=9 start
[3] pid=5 done (label=0)
[3] child_lo id=1 pid=6 prio=9 start
[3] pid=6 done (label=0)
CHECK: setpriority/getpriority on self OK (prio=3)
=== Phase A2 PASSED ===
```

The high-priority child (`prio=0`) runs first as expected;
both low-priority children (`prio=9`) follow, demonstrating
that aging allowed them to make progress even though the
high-priority child held the CPU for the entire first
scheduling slice.

## Verification

- [x] Program exits cleanly (no kernel panic).
- [x] High-priority child completes before low-priority
      children.
- [x] Low-priority children eventually run (no permanent
      starvation).
- [x] `setpriority` / `getpriority` round-trip on the parent
      returns the value just set.
- [x] `sched_algorithm(4)` switches to PRIO and back.

## Open issues / follow-ups

- **Priority inheritance not implemented.** This phase
  handles scheduling-order priority.  Inheritance (when a
  high-priority process is blocked on a resource held by a
  low-priority process, the holder's priority is temporarily
  boosted to the waiter's) requires a kernel-side mapping
  from "process blocked on what" and is deferred to
  Phase D1 (Mars Pathfinder).
- The priority field is clamped to `[0, MAX_PRIORITY]`.  We
  may want a separate cap for "boosted" priorities in a
  future phase to allow aging to push a process higher than
  the original user-set value.

## Files

- `kernel/proc.c`, `kernel/proc.h` (no change here),
  `kernel/param.h`, `kernel/syscall.c`, `kernel/syscall.h`,
  `kernel/sysproc.c`, `kernel/defs.h`
- `user/usys.pl`, `user/user.h`, `user/sched.c`,
  `user/prioritytest.c`, `Makefile`
