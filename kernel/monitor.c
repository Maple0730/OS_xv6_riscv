// Monitor (Phase C1)
//
// A monitor in Hoare's style bundles:
//   * a single mutex  (sem_id mutex)
//   * zero or more condition variables (each backed by a sem)
//
// API exposed via 6 system calls (sys_mon_create, sys_mon_lock,
// sys_mon_unlock, sys_mon_wait, sys_mon_signal, sys_mon_broadcast).
//
// We implement a Mesa-style monitor on top of xv6 semaphores:
//
//   mon_lock:
//     sem_wait(m->mutex);
//
//   mon_unlock:
//     sem_post(m->mutex);
//
//   mon_wait(mid, cvid):
//     sem_post(m->mutex);
//     sem_wait(m->cv[cvid]);
//     sem_wait(m->mutex);
//
//   mon_signal(mid, cvid):
//     sem_post(m->cv[cvid]);  // wakes ONE waiter if any
//
//   mon_broadcast(mid, cvid):
//     sem_broadcast(m->cv[cvid]);  // wakes ALL waiters
//
// Each monitor is identified by an integer id in [0, NMON).  A
// monitor occupies NRES_CV semaphores in the system semaphore
// table — one for the mutex and NRES_CV-1 for the condition
// variables.  The mapping is recorded here so we can recover the
// condition-variable sem id from (mid, cvid) without storing the
// mapping in user space.

#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sem.h"

#define NMON       8         // max monitors
#define NRES_CV    4         // 1 mutex + 3 condition variables per monitor

struct monitor {
  int allocated;            // 1 = in use
  int mutex;                // sem id of the mutex
  int cv[NRES_CV - 1];      // sem ids of the NRES_CV-1 condition vars
};

static struct monitor montable[NMON];
static struct spinlock mon_lock_inst;

int
monitor_init(void)
{
  initlock(&mon_lock_inst, "monitor");
  for (int i = 0; i < NMON; i++) {
    montable[i].allocated = 0;
  }
  return 0;
}

// Reserve the next K semaphores from the semaphore table.  Returns
// the first sem id in *out_first, or -1 on failure.
static int
alloc_sem_block(int k, int *out_first)
{
  int run_start = -1;
  for (int i = 0; i + k <= NSEM; i++) {
    int ok = 1;
    for (int j = 0; j < k; j++) {
      if (semtable[i + j].allocated) { ok = 0; break; }
    }
    if (ok) { run_start = i; break; }
  }
  if (run_start < 0) return -1;

  for (int j = 0; j < k; j++) {
    semtable[run_start + j].allocated = 1;
  }
  *out_first = run_start;
  return 0;
}

int
monitor_create(void)
{
  int mid = -1;
  acquire(&mon_lock_inst);
  for (int i = 0; i < NMON; i++) {
    if (!montable[i].allocated) { mid = i; break; }
  }
  if (mid < 0) { release(&mon_lock_inst); return -1; }

  int first;
  if (alloc_sem_block(NRES_CV, &first) < 0) {
    release(&mon_lock_inst);
    return -1;
  }

  montable[mid].allocated = 1;
  montable[mid].mutex = first;
  for (int j = 0; j < NRES_CV - 1; j++) {
    montable[mid].cv[j] = first + 1 + j;
  }
  // Initialise: mutex=1, cvs=0
  sem_init(first, 1);
  for (int j = 0; j < NRES_CV - 1; j++) {
    sem_init(first + 1 + j, 0);
  }
  release(&mon_lock_inst);
  return mid;
}

int
monitor_lock(int mid)
{
  if (mid < 0 || mid >= NMON) return -1;
  acquire(&mon_lock_inst);
  if (!montable[mid].allocated) { release(&mon_lock_inst); return -1; }
  int mutex = montable[mid].mutex;
  release(&mon_lock_inst);
  return sem_wait(mutex);
}

int
monitor_unlock(int mid)
{
  if (mid < 0 || mid >= NMON) return -1;
  acquire(&mon_lock_inst);
  if (!montable[mid].allocated) { release(&mon_lock_inst); return -1; }
  int mutex = montable[mid].mutex;
  release(&mon_lock_inst);
  return sem_post(mutex);
}

int
monitor_wait(int mid, int cvid)
{
  if (mid < 0 || mid >= NMON) return -1;
  if (cvid < 0 || cvid >= NRES_CV - 1) return -1;
  acquire(&mon_lock_inst);
  if (!montable[mid].allocated) { release(&mon_lock_inst); return -1; }
  int mutex = montable[mid].mutex;
  int cv    = montable[mid].cv[cvid];
  release(&mon_lock_inst);
  // Hoare-style: release mutex BEFORE waiting on cv, then re-acquire
  // mutex on return.  The Mesa variant (which we use here) does
  // not transfer the lock atomically; signal() bumps the cv sem
  // and the woken thread races to re-acquire the mutex.
  sem_post(mutex);
  sem_wait(cv);
  sem_wait(mutex);
  return 0;
}

int
monitor_signal(int mid, int cvid)
{
  if (mid < 0 || mid >= NMON) return -1;
  if (cvid < 0 || cvid >= NRES_CV - 1) return -1;
  acquire(&mon_lock_inst);
  if (!montable[mid].allocated) { release(&mon_lock_inst); return -1; }
  int cv = montable[mid].cv[cvid];
  release(&mon_lock_inst);
  return sem_post(cv);
}

int
monitor_broadcast(int mid, int cvid)
{
  if (mid < 0 || mid >= NMON) return -1;
  if (cvid < 0 || cvid >= NRES_CV - 1) return -1;
  acquire(&mon_lock_inst);
  if (!montable[mid].allocated) { release(&mon_lock_inst); return -1; }
  int cv = montable[mid].cv[cvid];
  release(&mon_lock_inst);
  sem_broadcast(cv);
  return 0;
}
