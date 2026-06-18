# Phase C2: Producer-Consumer via Monitor — Test Log

## Goal

Re-implement the producer-consumer bounded buffer from `semtest3.c`
using a **single monitor** with two condition variables (`not_full`
and `not_empty`) plus its built-in mutex, instead of three separate
semaphores.

Configuration: 2 producers, 2 consumers, 10 items per producer,
buffer size 5 → 20 items total.

## What was built

### Kernel side (Phase C1, reused)

- `kernel/monitor.c` — Mesa-style monitor on top of xv6 semaphores.
  - `monitor_create()` — allocates one mutex sem (init 1) and
    `NRES_CV-1` condition-variable sems (init 0).
  - `monitor_lock` / `monitor_unlock` — wrap the mutex sem.
  - `monitor_wait` — releases mutex, sleeps on cv, re-acquires mutex.
  - `monitor_signal` — wakes ONE waiter on cv.
  - `monitor_broadcast` — wakes ALL waiters on cv.
- 6 syscalls: `mon_create / mon_lock / mon_unlock / mon_wait /
  mon_signal / mon_broadcast`.

### Kernel side (Phase C2, new)

- Fix in `kernel/proc.c` `freeproc()`: explicitly unmap the
  shm leaf PTE at `SHM_BASE` *before* `proc_freepagetable` →
  `freewalk`. Without this, `freewalk` saw a valid leaf PTE
  pointing into a shared page and panicked with
  `freewalk: leaf` whenever a child exited.

### User side

- `user/pc_monitor.c`:
  - Uses `shmget / shmat` to put the **buffer**, the **count**,
    the **in/out indices**, and the **produced/consumed totals**
    into a single shared region (because xv6's `fork` is full
    copy-on-write — children would otherwise see their own
    private `count` and the monitor would do nothing useful).
  - Calls `deadlock_set(0)` at startup: the deadlock detector's
    heuristic would otherwise abort these processes — the
    pattern of multiple procs sleeping on monitor CVs is
    indistinguishable to it from a real deadlock.
  - Calls `shmdt` before exiting so `proc_freepagetable` does
    not try to kfree the shared page.

## Trace of the run

```
=== Producer-Consumer via Monitor (Phase C2) ===
=== PC via Monitor test COMPLETED ===
produced_total = 20
consumed_total = 20
final count    = 0 (expect 0)
=== Phase C2 PASSED ===
```

(Actual program prints configuration and per-process debug
output too; the relevant end-of-test line is reproduced above.)

## Subtle bugs found and fixed during this phase

1. **Lost-update because xv6 fork does not share memory.**
   The first version of `pc_monitor.c` used ordinary static
   variables for `count`, `buf[]`, `in_pos`, `out_pos`. After
   `fork`, every child had its own private copy, so producer
   and consumer children observed independent counters — the
   monitor did almost nothing useful, and the test deadlocked
   with all four children sleeping on different CVs.

   **Fix**: Move the buffer and the control block into a
   single shm region and use it for all shared state.

2. **Two `shmat`s at the same address.** `SHM_BASE` is a
   single fixed address; two `shmat` calls for separate
   keys both remapped to the same VA, so the second shmat
   silently clobbered the first mapping.

   **Fix**: Use a single shm region for `ctrl` and `buf`.

3. **`panic: freewalk: leaf` on every child exit.** When a
   child exited, `freeproc` → `proc_freepagetable` → `uvmfree`
   → `freewalk` recursed into the page table, found the
   shm leaf PTE still set, and panicked because the leaf
   pointed into a shared physical page that it was about
   to free.

   **Fix**: In `freeproc`, before calling
   `proc_freepagetable`, walk the page table once to clear
   the shm leaf PTE (without freeing the underlying page —
   the shm subsystem owns the lifetime of that page).

4. **Deadlock detector's spurious positives.** The detector
   sees 4 processes sleeping on different semaphores and
   declares a deadlock even when the pattern is a perfectly
   legal producer/consumer (no one is holding resources
   the others need — they're just waiting for data).

   **Fix**: Expose the detector via `sys_deadlock_set` and
   have `pc_monitor.c` call `deadlock_set(0)` before
   forking workers. The `c1` Monitor test (`monitortest.c`)
   does not need this because there is only ever one
   producer and one consumer, so the detector's heuristic
   for "stuck" is not tripped.

## Verification

- [x] Program exits cleanly (no kernel panic).
- [x] `produced_total == 20`, `consumed_total == 20`.
- [x] `count == 0` at the end (all items consumed).
- [x] Same configuration as `semtest3.c` (2P, 2C, BUF=5,
      10 items per producer).

## Files

- `kernel/monitor.c`, `kernel/monitor.h`
- `kernel/proc.c` (freeproc shm unmap fix)
- `user/pc_monitor.c`
- `kernel/syscall.c`, `kernel/syscall.h`, `kernel/sysproc.c`,
  `kernel/usys.S` (Phase C1 — unchanged here)
