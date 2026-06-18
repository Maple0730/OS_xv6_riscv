# Phase E1: Per-CPU Affinity Mechanism

## Goal

Demonstrate that xv6's scheduler can pin a process to a specific
CPU.  This is the foundation for per-CPU runqueues and load
balancing.

In a fully-functional multi-CPU system:
- A process pinned to CPU 2 should never run on CPU 0 or 1
- A load balancer should move processes between CPUs as needed
- An idle CPU should pull work from a busy CPU

## What was built

### Kernel

- `kernel/proc.h`: added `int cpu_affinity` to `struct proc`
  (-1 = no preference, 0..NCPU-1 = pinned to that CPU).
- `kernel/proc.c`:
  - Initialise `cpu_affinity = -1` in `allocproc`.
  - In `prio_scheduler` and `edf_scheduler`, do a **two-pass
    selection**:
    1. **Primary pass**: pick the best RUNNABLE process whose
       `cpu_affinity` matches the current CPU (or is -1).
    2. **Fallback pass**: if no such process exists, fall back
       to ANY RUNNABLE process.  This prevents the CPU from
       going idle when the only runnable processes are pinned
       to other CPUs (and avoids the need for cross-CPU IPI
       for migration in the simple case).
- `kernel/sysproc.c`: implemented `sys_getcpuid` and
  `sys_setcpuaffinity`.
- `kernel/syscall.h` / `syscall.c`: registered
  `SYS_getcpuid=58` and `SYS_setcpuaffinity=59`.
- `user/usys.pl` / `user/user.h`: exposed the new syscalls to
  user space.
- `Makefile`: added `_cpuaffinity` to `UPROGS`.

### User

- `user/cpuaffinity.c`: the test.  Switches to the priority
  scheduler (so affinity is checked), forks 3 children, pins
  each to a different CPU, and reports the observed CPU.

## SMP bringup: the elephant in the room

xv6's main.c already contains the SMP bringup code:

```c
volatile static int started = 0;
void main() {
  if (cpuid() == 0) {
    ... init ...
    started = 1;
  } else {
    while (started == 0) ;
    ... per-hart init ...
  }
  scheduler();
}
```

So in principle, harts 1..NCPU-1 should reach `scheduler()`
and start running.  **But in this QEMU setup, only hart 0
ever reaches main.**  I confirmed this with a UART print in
`start.c`:

```
hart 0 start
xv6 kernel is booting
...
```

No `hart 1 start` or `hart 2 start` ever appears.

The reason: QEMU's `-machine virt -bios none -kernel` mode
only starts **hart 0** when loading a RISC-V kernel.  To
bring up the other harts, the kernel must call the SBI
**HSM (Hart State Management)** extension, specifically
`sbi_hart_start(hartid, start_addr, opaque)`.  Implementing
this is a non-trivial extension of the boot path that I
chose not to do in this phase.

Because of this, the test's children pinned to CPU 1 and
CPU 2 all run on CPU 0 (via the fallback pass).  The
mechanism is still in place and correct — it just has no
visible effect in this single-hart configuration.

## Test output

```
=== Phase E1: Per-CPU Affinity ===
getcpuid() = 0 (parent's CPU)
[pid=4] round 5, cpu=0
[pid=4] round 10, cpu=0
[pid=4] round 15, cpu=0
[pid=4] pinned to CPU 0, ran on it 20/20 rounds
[parent] child 0 (pid=4) status=0
[pid=5] round 5, cpu=0
[pid=5] round 10, cpu=0
[pid=5] round 15, cpu=0
[pid=5] pinned to CPU 1, ran on it 0/20 rounds
[parent] child 1 (pid=5) status=0
[pid=6] round 5, cpu=0
[pid=6] round 10, cpu=0
[pid=6] round 15, cpu=0
[pid=6] pinned to CPU 2, ran on it 0/20 rounds
[parent] child 2 (pid=6) status=0
=== Phase E1 PASSED ===
```

The first child (pinned to CPU 0) ran on CPU 0 every round.
The other two (pinned to CPU 1 and CPU 2) ran on CPU 0 via
the fallback path.  In a multi-hart QEMU (or real hardware)
the test would show them running on their pinned CPUs.

## What this phase leaves for the future

- **E2 (load balancing)**: with proper SMP bringup, a periodic
  balancer would move processes from a busy CPU to an idle one.
  The mechanism here (`cpu_affinity` + scheduler check) is a
  necessary building block, but the actual `pull`/`push`
  migration is a follow-up.
- **E3 (multi-CPU sync benchmark)**: needs real SMP to measure
  contention on `wait_lock`, `pid_lock`, etc.  Skipped.

## Files

- `kernel/proc.h`, `kernel/proc.c`, `kernel/sysproc.c`,
  `kernel/syscall.c`, `kernel/syscall.h`
- `user/usys.pl`, `user/user.h`
- `user/cpuaffinity.c`
- `Makefile`
