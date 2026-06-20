// Phase G2: kernel-thread (kthread) framework
//
// A kernel thread is a lightweight task that runs in supervisor mode only:
//   - no user page table
//   - no user trapframe
//   - no user memory (.text/.data segment)
//   - context.sp is the kernel stack, context.ra is the entry point
//
// It still appears as a struct proc, so the existing scheduler
// (RR/MLFQ/Stride/Lottery) treats it like any other RUNNABLE entity.
//
// The kthread API is intentionally minimal, matching Linux's
// kthread_create / kthread_should_stop / kthread_stop trio:
//
//   struct proc *p = kthread_create(my_fn, arg, "myk");
//   // ... later, perhaps in another context ...
//   kthread_stop(p);            // sets p->kthread_should_stop and wakes it up
//   wait(p);                    // reap (kthread doesn't kexit, so
//                                // its parent must wait)
//
// my_fn's last statement should be kthread_exit().
//
// Two demo kthreads are spawned from main.c:
//   - reaperd       : every 100 ticks, scan proc table for orphan ZOMBIEs
//                     whose parent (init) is busy, and reap them.
//   - schedstat_dump: every 200 ticks, dump current_scheduler and
//                     selected procs' run_time/sched_count.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

// Trampoline: every kthread starts here, then jumps to fn(arg).
// fn must eventually call kthread_exit(), which calls scheduler() (via yield).
static void
kthread_trampoline(void)
{
  struct proc *p = myproc();
  void (*fn)(void *) = (void (*)(void *))p->kthread_arg;
  void *arg = p->kthread_arg_user;
  // Enable interrupts: swtch() disabled them.
  intr_on();
  fn(arg);
  kthread_exit();
}

// Create a kernel thread running fn(arg) with given name.
// Returns the new proc, or 0 on failure.
// The returned proc is RUNNABLE and will be scheduled.
struct proc *
kthread_create(void (*fn)(void *), void *arg, const char *name)
{
  struct proc *p;

  // Allocate a proc slot.  allocproc normally allocates trapframe +
  // user pagetable; we don't want either, so we roll back if needed.
  // Simpler path: walk proc[] manually, doing only what we need.
  acquire(&wait_lock);
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == UNUSED)
      goto found;
    release(&p->lock);
  }
  release(&wait_lock);
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;
  p->is_kthread = 1;
  p->parent = initproc;
  p->ctime = ticks;
  p->queue_level = 0;
  p->timeslice_used = 0;
  p->last_sched = 0;
  p->priority = DEFAULT_PRIORITY;
  p->orig_priority = DEFAULT_PRIORITY;
  p->boost_count = 0;
  p->cpu_affinity = -1;
  p->est_burst = SJF_DEFAULT_BURST;
  p->cow_faults = 0;
  p->cow_pages_total = 0;
  p->weight = STRIDE_DEF_W;
  p->stride = STRIDE_BIG / p->weight;
  p->pass = 0;

  // No trapframe, no user pagetable for kthreads.
  p->trapframe = 0;
  p->pagetable = 0;
  p->sz = 0;

  // Link into initproc's child list (so wait() can reap it).
  p->cnext = initproc->cnext;
  if (initproc->cnext)
    initproc->cnext->cprev = p;
  initproc->cnext = p;
  p->cprev = 0;
  initproc->child_count++;

  // Save fn/arg on the proc struct itself for trampoline to pick up.
  p->kthread_arg = (uint64)fn;
  p->kthread_arg_user = arg;
  p->kthread_should_stop = 0;
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)kthread_trampoline;
  p->context.sp = p->kstack + PGSIZE;

  // Adopt into sched: mark RUNNABLE.
  safestrcpy(p->name, name, sizeof(p->name));
  p->state = RUNNABLE;

  release(&p->lock);
  release(&wait_lock);
  return p;
}

// Called inside the kthread's body to check if it should stop.
// Returns nonzero if kthread_stop() has been called.
int
kthread_should_stop(void)
{
  struct proc *p = myproc();
  return p->kthread_should_stop;
}

// Called inside the kthread's body to terminate.
// Goes to ZOMBIE; parent's wait() can then reap.
void
kthread_exit(void)
{
  struct proc *p = myproc();

  acquire(&wait_lock);
  acquire(&p->lock);
  p->state = ZOMBIE;
  p->xstate = 0;
  // wake initproc (or whichever parent is waiting)
  wakeup(initproc);
  release(&p->lock);
  release(&wait_lock);

  // Jump into scheduler; will never return.
  sched();
  panic("kthread_exit returned");
}

// Request a kthread to stop.
// Wakes it up if it is sleeping in kthread_should_stop polling.
void
kthread_stop(struct proc *p)
{
  acquire(&p->lock);
  p->kthread_should_stop = 1;
  // If the kthread is sleeping on something, kick it.
  wakeup(p);
  release(&p->lock);
}

// Helper: sleep for `ticks_to_wait` ticks using a private chan.
// Caller's lock (none, since we use a dedicated chan) is not held.
static void
kthread_sleep(int ticks_to_wait)
{
  uint64 start = ticks;
  acquire(&tickslock);
  while (ticks - start < (uint64)ticks_to_wait) {
    sleep(&ticks, &tickslock); // sleep on ticks (semantic chan)
  }
  release(&tickslock);
}

// ----- demo kthreads -----

// reaperd: every ~100 ticks, scan for orphan ZOMBIEs whose parent is
// initproc but init has more than 1 child outstanding.  In xv6 this is
// currently done by init's main loop; reaperd demonstrates a parallel
// background reaper.  It just prints stats, since actually reaping
// requires holding wait_lock for a long scan.
static void
reaperd_fn(void *arg)
{
  (void)arg;
  int cycles = 0;

  while (!kthread_should_stop()) {
    kthread_sleep(100);

    if (kthread_should_stop()) break;

    int total = 0, zombies = 0, sleeping = 0, running = 0;
    struct proc *q;
    for (q = proc; q < &proc[NPROC]; q++) {
      if (q->state == UNUSED || q->state == USED) continue;
      total++;
      if (q->state == ZOMBIE) zombies++;
      if (q->state == SLEEPING) sleeping++;
      if (q->state == RUNNING) running++;
    }
    printf("[reaperd] cycle=%d total=%d zombies=%d sleeping=%d running=%d scheduler=%s\n",
           cycles++, total, zombies, sleeping, running,
           sched_algo_name(current_scheduler));
  }
  printf("[reaperd] exiting\n");
  kthread_exit();
}

// schedstat_dump: every ~200 ticks, dump the running process and its stats.
// Demonstrates that kthreads participate in scheduling.
static void
schedstat_dump_fn(void *arg)
{
  (void)arg;
  int cycles = 0;

  while (!kthread_should_stop()) {
    kthread_sleep(200);

    if (kthread_should_stop()) break;

    struct proc *p = myproc();
    printf("[schedstat] cycle=%d scheduler=%s pid=%d name=%s is_kthread=%d run_time=%lu cow_faults=%lu\n",
           cycles++, sched_algo_name(current_scheduler),
           p->pid, p->name, p->is_kthread,
           p->run_time, p->cow_faults);
  }
  printf("[schedstat] exiting\n");
  kthread_exit();
}

// Public entry: spawn both demo kthreads.
void
kthread_init(void)
{
  printf("[kthread] starting reaperd and schedstat_dump\n");
  kthread_create(reaperd_fn, 0, "reaperd");
  kthread_create(schedstat_dump_fn, 0, "schedstat");
}