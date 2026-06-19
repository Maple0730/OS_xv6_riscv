#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "sem.h"
#include "defs.h"

struct semaphore semtable[NSEM];

// Initialize the semaphore table
void
seminit(void)
{
  for (int i = 0; i < NSEM; i++) {
    initlock(&semtable[i].lock, "semaphore");
    semtable[i].value = 0;
    semtable[i].allocated = 0;
    semtable[i].waiters = 0;
    semtable[i].name[0] = 0;
    semtable[i].holder_pid = -1;
  }
}

// Find a free semaphore slot
static int
sem_alloc(void)
{
  for (int i = 0; i < NSEM; i++) {
    if (!semtable[i].allocated) {
      return i;
    }
  }
  return -1;
}

// Initialize a semaphore with a given value
int
sem_init(int sem_id, int value)
{
  if (sem_id < 0 || sem_id >= NSEM) {
    return -1;
  }

  struct semaphore *sem = &semtable[sem_id];
  acquire(&sem->lock);
  sem->value = value;
  sem->allocated = 1;
  sem->waiters = 0;
  release(&sem->lock);

  return 0;
}

// Open (allocate) a new semaphore with initial value
int
sem_open(int *sem_id, int value)
{
  int id = sem_alloc();
  if (id < 0) {
    return -1;  // No free semaphore slots
  }

  *sem_id = id;
  return sem_init(id, value);
}

// P operation: decrement semaphore, block if <= 0
int
sem_wait(int sem_id)
{
  if (sem_id < 0 || sem_id >= NSEM) {
    return -1;
  }

  struct semaphore *sem = &semtable[sem_id];

  acquire(&sem->lock);

  if (!sem->allocated) {
    release(&sem->lock);
    return -1;
  }

  // Phase D1: priority inheritance.  If this sem is held
  // (value<1) by another process, and our priority is higher
  // (lower number), boost the holder's priority to ours.
  // This prevents the classic "priority inversion" scenario
  // where a medium-priority CPU-bound process pre-empts a
  // low-priority process that is holding a resource needed
  // by a high-priority process.
  int my_pid = myproc()->pid;
  if (sem->value < 1 && sem->holder_pid >= 0 && sem->holder_pid != my_pid) {
    struct proc *holder = 0;
    for (struct proc *q = proc; q < &proc[NPROC]; q++) {
      if (q->pid == sem->holder_pid) { holder = q; break; }
    }
    if (holder) {
      acquire(&holder->lock);
      int my_prio = myproc()->priority;
      if (my_prio < holder->priority) {
        if (holder->boost_count == 0)
          holder->orig_priority = holder->priority;
        holder->priority = my_prio;
        holder->boost_count++;
      }
      release(&holder->lock);
    }
  }

  sem->value--;
  if (sem->value < 0) {
    // Must sleep to wait for V operation
    sleep(sem, &sem->lock);
  } else {
    // We just acquired the sem (value went from 1 to 0, or
    // it was 0 and a single post was issued, etc.).  Record
    // ourselves as the holder so the next waiter can apply
    // priority inheritance.
    sem->holder_pid = my_pid;
  }

  release(&sem->lock);
  return 0;
}

// V operation: increment semaphore, wake up a waiter if any
int
sem_post(int sem_id)
{
  if (sem_id < 0 || sem_id >= NSEM) {
    return -1;
  }

  struct semaphore *sem = &semtable[sem_id];

  acquire(&sem->lock);

  if (!sem->allocated) {
    release(&sem->lock);
    return -1;
  }

  sem->value++;
  if (sem->value <= 0) {
    // There are waiters — clear our holder record before
    // waking one.  The woken waiter will set its own
    // holder_pid in sem_wait()'s else branch.
    sem->holder_pid = -1;
    wakeup(sem);
  }
  // Phase D1: restore our priority if it was boosted — do
  // this regardless of whether there were waiters.  If we
  // owned the sem (value was 0 or below before our post),
  // we are no longer the holder, so the boost (if any)
  // should be released.
  struct proc *p = myproc();
  acquire(&p->lock);
  if (p->boost_count > 0) {
    p->boost_count--;
    if (p->boost_count == 0)
      p->priority = p->orig_priority;
  }
  release(&p->lock);

  release(&sem->lock);

  return 0;
}

// Get the current value of a semaphore
int
sem_get(int sem_id, int *value)
{
  if (sem_id < 0 || sem_id >= NSEM) {
    return -1;
  }

  struct semaphore *sem = &semtable[sem_id];

  acquire(&sem->lock);
  if (!sem->allocated) {
    release(&sem->lock);
    return -1;
  }
  *value = sem->value;
  release(&sem->lock);

  return 0;
}

// Close (deallocate) a semaphore
int
sem_close(int sem_id)
{
  if (sem_id < 0 || sem_id >= NSEM) {
    return -1;
  }

  struct semaphore *sem = &semtable[sem_id];

  acquire(&sem->lock);
  if (!sem->allocated) {
    release(&sem->lock);
    return -1;
  }

  // Wake up all waiters
  while (sem->waiters != 0 || sem->value < 0) {
    wakeup(sem);
  }

  sem->allocated = 0;
  sem->value = 0;
  sem->waiters = 0;
  release(&sem->lock);

  return 0;
}

// Wake ALL waiters on this sem (Phase C1: monitor broadcast).
// Bumps value by N where N = -value (i.e. unblocks every waiter).
void
sem_broadcast(int sem_id)
{
  if (sem_id < 0 || sem_id >= NSEM) return;
  struct semaphore *sem = &semtable[sem_id];
  acquire(&sem->lock);
  if (!sem->allocated) {
    release(&sem->lock);
    return;
  }
  // Compute how many waiters there are (the difference between
  // -value and the number of wakeups we will issue).
  // The xv6 semaphore only tracks `value` and `waiters` list, not
  // a precise count of sleeping procs.  We loop calling wakeup
  // until there are no more waiters (or a safety cap is hit).
  int safety = NSEM + 4;
  while (safety-- > 0) {
    if (sem->waiters == 0 && sem->value >= 0) break;
    sem->value++;
    wakeup(sem);
  }
  release(&sem->lock);
}
