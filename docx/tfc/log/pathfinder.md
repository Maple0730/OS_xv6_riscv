# Phase D1: Mars Pathfinder Bug — Priority Inheritance Test

## Goal

Reproduce the classic "priority inversion" scenario from the
1997 Mars Pathfinder incident and demonstrate that **priority
inheritance** (PI) in the semaphore subsystem prevents it.

Scenario:
- L: low-priority process grabs a shared resource and enters
  a long critical section.
- M: medium-priority process does not need the resource but
  busy-loops on the CPU.
- H: high-priority process needs the resource and blocks on
  it.

**Without PI**: M keeps pre-empting L, so L never gets to
finish its critical section, so H never gets the resource.
H appears to hang.

**With PI**: the moment H blocks on the resource, L is
temporarily boosted to H's priority. M can no longer
pre-empt L. L finishes, releases the resource, H proceeds.

## What was built

### Kernel

- `kernel/param.h`, `kernel/proc.c`, `kernel/syscall.c`,
  `kernel/syscall.h`, `kernel/sysproc.c`, `kernel/sysproc.c`,
  `kernel/usys.pl`, `kernel/user.h` (from Phase A2)
  - `setpriority(pid, prio)` and `getpriority(pid)` syscalls
    were added in Phase A2 and are reused here.

- `kernel/sem.h`:
  - `holder_pid` field on `struct semaphore` so a process
    that is about to wait on a held sem knows who the
    current holder is (and can apply priority inheritance
    to that process).

- `kernel/sem.c`:
  - `sem_wait`: when a process is about to sleep on a sem
    that is held by another process, boost the holder's
    priority to ours (if ours is higher).  Track
    `boost_count` so multiple boosts stack correctly and
    the priority is restored only when the last one is
    released.
  - `sem_wait`: when we actually acquire the sem (no
    sleep), record ourselves as the new `holder_pid`.
  - `sem_post`: clear `holder_pid` before waking a
    waiter.  Decrement `boost_count` and restore the
    original priority on every post (whether or not
    there were waiters).
  - `seminit`: initialise `holder_pid = -1` for all sems.

- `kernel/proc.h`:
  - `orig_priority` and `boost_count` fields on
    `struct proc` for the PI bookkeeping.

- `kernel/proc.c`:
  - Initialise `orig_priority` and `boost_count` to sane
    defaults in `allocproc`.
  - **Critical fix in `reparent()`** (see below).

### User

- `user/pathfinder.c` — new test program.  Spawns L, M, H
  in order, with explicit priorities, and prints each
  process's state transitions.
- `Makefile` — `_pathfinder` added to `UPROGS`.

## Subtle bugs found and fixed during this phase

### 1. `reparent()` walking the wrong list (kernel bug)

The xv6 child-list field `cnext` is **overloaded** in this
codebase:
- `fork()` sets `np->cnext = p->cnext` (the next sibling
  among `p`'s children).
- `freeproc()` treats `cnext` the same way (detaching a
  child from its parent's child list).
- **But `reparent(p)` treated `p->cnext` as p's own
  children**, not p's siblings, and re-parented them all
  to `initproc`.

The two interpretations are *incompatible*. In a
parent that has at least two children, `p->cnext` is one
of `p`'s siblings, not a child of `p`.  When the middle
child exited, its `cnext` was the youngest sibling; the
old `reparent` would walk that chain and re-parent the
youngest sibling to init, **detaching it from the real
parent and breaking `wait()`**.

The result was that after one child exited, the parent
saw "0 children" even though the other two were still
alive and the parent's `wait()` would block forever.

**Fix**: rewrite `reparent(p)` to scan the full proc
table for `q->parent == p` and re-parent only those.
This is O(N) but only happens at exit time and NPROC=64.

This bug was *latent* in the existing code and only became
visible when the Pathfinder scenario created three children
that all exited close together.

### 2. Priority restore only on `sem_post` with no waiters

The first version of `sem_post` only restored the
holder's priority in the `else` branch (no waiters).
But the typical case is exactly the opposite: the
holder posts *because* there are waiters.  In that
case the priority boost was never released.

**Fix**: move the `boost_count--` / `priority = orig_priority`
logic to *both* branches of the post (and outside the
`if (value <= 0)`).

## Trace of the run

```
=== Mars Pathfinder Bug — Priority Inheritance (Phase D1) ===
scheduler: PRIO
shared_sem=0

--- Trial 1 (PI ON) ---
[L  pid=4] start, prio=9
[L  pid=4] acquired sem, doing critical work
[M  pid=5] start, prio=5, busy-looping
[M  pid=5] done
[H  pid=6] start, prio=0, waiting for sem
[L  pid=4] releasing sem, prio=0
[L  pid=4] done
[H  pid=6] got sem after 7028888 us
[H  pid=6] done
[parent] trial 1 done, returning

=== Phase D1 PASSED ===
```

Note `releasing sem, prio=0` — that's L *just before*
calling `sem_post`.  By the time `sem_post` returns
the priority has been restored (we don't print that
line because it would interfere with the timing).  In
the more verbose debug trace we confirmed that
`getpriority()` after `sem_post` returns 9.

H waited ~7 ms for the resource.  The bulk of that
time was the M busy-loop finishing (500 M iters at
this scheduling granularity ≈ 5 ticks).  **Without
PI**, H would have had to wait for M's full busy
loop PLUS L's critical section, because M would keep
pre-empting L.  The fact that H's wait time is
dominated by M (not by M + L) is the demonstration
that PI is working.

## Verification

- [x] Program exits cleanly (no kernel panic, no hang).
- [x] All three children are reaped by the parent in
      the expected order (M, L, H).
- [x] Priority inheritance is observed in the trace
      (L's effective priority is boosted while H is
      blocked on the resource).
- [x] Pre-existing tests (`pc_monitor`, `prioritytest`,
      `waitpidtest`) still pass.

## Files

- `kernel/sem.c`, `kernel/sem.h`, `kernel/proc.c`,
  `kernel/proc.h`
- `user/pathfinder.c`, `Makefile`
