#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "banker.h"

extern struct proc proc[];

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_waitpid(void)
{
  int pid;
  uint64 status_addr;

  argint(0, &pid);
  argaddr(1, &status_addr);

  return kwaitpid(pid, status_addr);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if (t == SBRK_EAGER || n < 0) {
    if (growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (addr + n < addr)
      return -1;
    if (addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// print process list
uint64
sys_ps(void)
{
  procdump();
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// shut down the machine: write to qemu's test device to exit qemu.
// the test device is at 0x100000 in qemu's virt machine;
// any write to it causes qemu to exit gracefully.
uint64
sys_halt(void)
{
  volatile uint32 *test_dev = (volatile uint32 *)TEST_DEVICE;
  *test_dev = 0x5555;
  return 0;
}

uint64
sys_sem_open(void)
{
  int value;
  argint(0, &value);

  int sem_id;
  int ret = sem_open(&sem_id, value);
  if (ret < 0) {
    return -1;
  }
  return sem_id;
}

uint64
sys_sem_wait(void)
{
  int sem_id;
  argint(0, &sem_id);
  return sem_wait(sem_id);
}

uint64
sys_sem_post(void)
{
  int sem_id;
  argint(0, &sem_id);
  return sem_post(sem_id);
}

uint64
sys_sem_get(void)
{
  int sem_id;
  uint64 addr;
  argint(0, &sem_id);
  argaddr(1, &addr);

  int value;
  int ret = sem_get(sem_id, &value);
  if (ret < 0) {
    return -1;
  }

  struct proc *p = myproc();
  if (copyout(p->pagetable, addr, (char *)&value, sizeof(value)) < 0) {
    return -1;
  }
  return 0;
}

uint64
sys_sem_close(void)
{
  int sem_id;
  argint(0, &sem_id);
  return sem_close(sem_id);
}

uint64
sys_shmdt(void)
{
  uint64 addr;
  argaddr(0, &addr);
  return shmdt(addr);
}

uint64
sys_shmget(void)
{
  int key, size, shmflg;
  argint(0, &key);
  argint(1, &size);
  argint(2, &shmflg);
  return shmget(key, size, shmflg);
}

uint64
sys_shmat(void)
{
  int key;
  uint64 addr;
  argint(0, &key);
  argaddr(1, &addr);

  uint64 shm_addr;
  int ret = shmat(key, &shm_addr);
  if (ret < 0)
    return -1;

  struct proc *p = myproc();
  if (copyout(p->pagetable, addr, (char *)&shm_addr, sizeof(shm_addr)) < 0)
    return -1;

  return 0;
}

// Change scheduling algorithm at runtime
// arg0: algo (0=RR, 1=FCFS, 2=MLFQ, 3=SJF, -1=query only)
// returns: current algorithm (or previous if changed), -1 on failure
uint64
sys_sched_algorithm(void)
{
  int algo;
  argint(0, &algo);

  // Query mode: algo == -1 returns current algorithm without changing
  if (algo == -1) {
    return current_scheduler;
  }

  // Validate algorithm number (0=RR, 1=FCFS, 2=MLFQ, 3=SJF, 4=PRIO)
  if (algo < 0 || algo > 5)
    return -1;

  acquire(&sched_lock);
  int old = current_scheduler;
  current_scheduler = algo;
  release(&sched_lock);

  // Force the current process to yield so the scheduler dispatcher can
  // re-read current_scheduler and switch to the new algorithm.
  // Skip if old == algo (no change) or if we're the boot process (no proc).
  if (algo != old && myproc() != 0)
    yield();

  return old;
}

// Set the estimated CPU burst for a process (used by SJF scheduler).
// arg0: pid
// arg1: estimated burst in ticks (1..SJF_MAX_BURST)
// returns: 0 on success, -1 on error
uint64
sys_sched_setburst(void)
{
  int pid;
  int est;
  argint(0, &pid);
  argint(1, &est);
  return ksched_setburst(pid, (uint64)est);
}

// Phase A2: setpriority(pid, prio)
// Set the static priority of a process (lower = higher priority).
// prio must be in [0, MAX_PRIORITY].  Returns 0 on success, -1 on error.
uint64
sys_setpriority(void)
{
  int pid;
  int prio;
  argint(0, &pid);
  argint(1, &prio);
  return ksched_setprio(pid, prio);
}

// Phase A2: getpriority(pid)
// Get the static priority of a process.  Returns the priority on
// success, -1 if the process does not exist.
uint64
sys_getpriority(void)
{
  int pid;
  struct proc *p;
  int found = -1;

  argint(0, &pid);
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->pid == pid && p->state != UNUSED) {
      found = p->priority;
    }
    release(&p->lock);
  }
  return found;
}

// Phase F1: rt_register(period, cost)
// Register the calling process as a real-time task with the given
// period (in ticks) and worst-case CPU cost per period.  Returns
// 0 on success, -1 on error.
uint64
sys_rt_register(void)
{
  int period, cost;
  struct proc *p = myproc();

  argint(0, &period);
  argint(1, &cost);
  if (period <= 0 || cost < 0 || cost > period)
    return -1;

  acquire(&p->lock);
  p->rt_period = period;
  p->rt_cost = cost;
  p->rt_release = ticks;
  p->rt_deadline = ticks + period;
  // RM: shorter period -> higher priority (lower number)
  // Map period 1 tick -> priority 0, period 100 ticks -> priority 7.
  // Use a simple logarithmic-ish mapping for visibility.
  int prio = 0;
  int ptmp = period;
  while (ptmp > 1 && prio < MAX_PRIORITY) {
    ptmp >>= 1;
    prio++;
  }
  if (p->boost_count == 0)
    p->priority = prio;
  p->orig_priority = prio;
  release(&p->lock);
  return 0;
}

// Phase F1: rt_wait_period()
// Block the calling process until the start of its next period.
// The caller is responsible for doing its work and then calling
// rt_wait_period() to yield to the next period.
uint64
sys_rt_wait_period(void)
{
  struct proc *p = myproc();
  uint64 next_release;

  acquire(&p->lock);
  if (p->rt_period == 0) {
    release(&p->lock);
    return -1;  // not a real-time task
  }
  next_release = p->rt_release + p->rt_period;
  p->rt_release = next_release;
  p->rt_deadline = next_release + p->rt_period;
  release(&p->lock);

  // Sleep until the next release.  We sleep on the process's
  // own rt_release field; the clock interrupt will wake it
  // when the deadline arrives.
  acquire(&tickslock);
  while (ticks < next_release) {
    if (killed(p)) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// Phase E1: getcpuid() — return the CPU id on which the
// calling process is currently running.
uint64
sys_getcpuid(void)
{
  return cpuid();
}

// Phase E1: setcpuaffinity(pid, cpuid) — pin a process to a
// specific CPU.  cpuid -1 means "no preference" (any CPU).
// The scheduler will respect the affinity when picking a
// RUNNABLE process.
uint64
sys_setcpuaffinity(void)
{
  int pid, cpuid;
  struct proc *p;
  int found = -1;

  argint(0, &pid);
  argint(1, &cpuid);
  if (cpuid < -1 || cpuid >= NCPU)
    return -1;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->pid == pid && p->state != UNUSED) {
      p->cpu_affinity = cpuid;
      found = 0;
    }
    release(&p->lock);
  }
  return found;
}

// Phase D2: msgget/msgsnd/msgrcv wrappers
uint64
sys_msgget(void)
{
  int key, size;
  argint(0, &key);
  argint(1, &size);
  return msgget(key, size);
}

uint64
sys_msgsnd(void)
{
  int qid, len;
  uint64 buf;
  argint(0, &qid);
  argaddr(1, &buf);
  argint(2, &len);
  return msgsnd(qid, (char *)buf, len);
}

uint64
sys_msgrcv(void)
{
  int qid, len;
  uint64 buf;
  argint(0, &qid);
  argaddr(1, &buf);
  argint(2, &len);
  return msgrcv(qid, (char *)buf, len);
}

// Get current scheduler algorithm (internal use)
int
get_sched_algorithm(void)
{
  return current_scheduler;
}

// settimeslice - set timeslice for a specific queue or RR/FCFS
// queue: -1=RR/FCFS, 0=MLFQ_Q0, 1=MLFQ_Q1, 2=MLFQ_Q2, 3=MLFQ_Q3, 4=MLFQ_Q4
// ticks: new timeslice in ticks
// returns 0 on success, -1 on error
uint64
sys_settimeslice(void)
{
  int queue;
  int ticks;
  argint(0, &queue);
  argint(1, &ticks);
  if (ticks <= 0)
    return -1;
  acquire(&timeslice_lock);
  if (queue == -1) {
    rr_fcfs_timeslice = ticks;
  } else if (queue >= 0 && queue < MLFQ_LEVELS) {
    timeslice_table[queue] = ticks;
  } else {
    release(&timeslice_lock);
    return -1;
  }
  release(&timeslice_lock);
  return 0;
}

// gettimeslice - get current timeslice for a specific queue or RR/FCFS
// queue: -1=RR/FCFS, 0=MLFQ_Q0, 1=MLFQ_Q1, 2=MLFQ_Q2, 3=MLFQ_Q3, 4=MLFQ_Q4
// returns timeslice in ticks, -1 on error
uint64
sys_gettimeslice(void)
{
  int queue;
  argint(0, &queue);
  if (queue == -1)
    return rr_fcfs_timeslice;
  if (queue >= 0 && queue < MLFQ_LEVELS)
    return timeslice_table[queue];
  return -1;
}

uint64
sys_cgettimeofday(void)
{
  return r_time();
}

// schedstat - return scheduling statistics for a process
// arg0: pid of process to query (0 = current process)
// arg1: user address to write stats struct
// returns 0 on success, -1 on failure
uint64
sys_schedstat(void)
{
  int pid;
  uint64 addr;
  argint(0, &pid);
  argaddr(1, &addr);

  struct proc *target;
  if (pid == 0) {
    target = myproc();
  } else {
    target = 0;
    for (int i = 0; i < NPROC; i++) {
      if (proc[i].pid == pid) {
        target = &proc[i];
        break;
      }
    }
  }

  if (target == 0)
    return -1;

  struct {
    int pid;
    int queue_level;
    int sched_count;
    uint64 wait_time;
    uint64 run_time;
  } stats;

  acquire(&target->lock);
  stats.pid = target->pid;
  stats.queue_level = target->queue_level;
  stats.sched_count = target->sched_count;
  stats.wait_time = target->wait_time;
  stats.run_time = target->run_time;
  release(&target->lock);

  if (copyout(myproc()->pagetable, addr, (char *)&stats, sizeof(stats)) < 0)
    return -1;

  return 0;
}

// ===== Banker's algorithm system calls (Phase B3) =====

// banker_init(nres, avail_ptr)
uint64
sys_banker_init(void)
{
  int nres;
  uint64 addr;
  argint(0, &nres);
  argaddr(1, &addr);
  if (nres <= 0 || nres > NRES) return -1;
  int avail[NRES];
  if (copyin(myproc()->pagetable, (char *)avail, addr, sizeof(int) * nres) < 0)
    return -1;
  return banker_init(nres, avail);
}

// banker_setmax(pid, max_ptr)
uint64
sys_banker_setmax(void)
{
  int pid;
  uint64 addr;
  argint(0, &pid);
  argaddr(1, &addr);
  if (pid < 0 || pid >= NPROC_B) return -1;
  int max[NRES];
  // We don't yet know nres at this point, so copy NRES ints.
  if (copyin(myproc()->pagetable, (char *)max, addr, sizeof(int) * NRES) < 0)
    return -1;
  return banker_setmax(pid, max);
}

// banker_request(pid, req_ptr)
uint64
sys_banker_request(void)
{
  int pid;
  uint64 addr;
  argint(0, &pid);
  argaddr(1, &addr);
  if (pid < 0 || pid >= NPROC_B) return -1;
  int req[NRES];
  if (copyin(myproc()->pagetable, (char *)req, addr, sizeof(int) * NRES) < 0)
    return -1;
  return banker_request(pid, req);
}

// banker_release(pid, rel_ptr)
uint64
sys_banker_release(void)
{
  int pid;
  uint64 addr;
  argint(0, &pid);
  argaddr(1, &addr);
  if (pid < 0 || pid >= NPROC_B) return -1;
  int rel[NRES];
  if (copyin(myproc()->pagetable, (char *)rel, addr, sizeof(int) * NRES) < 0)
    return -1;
  return banker_release(pid, rel);
}

// banker_safe_sequence(out_seq_ptr) -- writes NPROC_B ints
uint64
sys_banker_safe_sequence(void)
{
  uint64 addr;
  argaddr(0, &addr);
  int seq[NPROC_B];
  int r = banker_safe_sequence(seq);
  if (r < 0) return -1;
  if (copyout(myproc()->pagetable, addr, (char *)seq, sizeof(seq)) < 0)
    return -1;
  return 0;
}

// banker_get_state(out_state_ptr) -- writes struct banker_state
uint64
sys_banker_get_state(void)
{
  uint64 addr;
  argaddr(0, &addr);
  return banker_get_state(addr);
}

// banker_setmax_alloc(pid, max_ptr, alloc_ptr)
uint64
sys_banker_setmax_alloc(void)
{
  int pid;
  uint64 maddr, aaddr;
  argint(0, &pid);
  argaddr(1, &maddr);
  argaddr(2, &aaddr);
  if (pid < 0 || pid >= NPROC_B) return -1;
  int max[NRES], alloc[NRES];
  if (copyin(myproc()->pagetable, (char *)max,   maddr, sizeof(int) * NRES) < 0) return -1;
  if (copyin(myproc()->pagetable, (char *)alloc, aaddr, sizeof(int) * NRES) < 0) return -1;
  return banker_setmax_alloc(pid, max, alloc);
}

// ===== Monitor (Phase C1) =====
int mon_verbose = 0;
uint64
sys_mon_create(void)
{
  int r = monitor_create();
  if (mon_verbose) printf("[mon] create -> %d\n", r);
  return r;
}

uint64
sys_mon_lock(void)
{
  int mid;
  argint(0, &mid);
  int r = monitor_lock(mid);
  if (mon_verbose) printf("[mon] lock mid=%d -> %d\n", mid, r);
  return r;
}

uint64
sys_mon_unlock(void)
{
  int mid;
  argint(0, &mid);
  int r = monitor_unlock(mid);
  if (mon_verbose) printf("[mon] unlock mid=%d -> %d\n", mid, r);
  return r;
}

uint64
sys_mon_wait(void)
{
  int mid, cvid;
  argint(0, &mid);
  argint(1, &cvid);
  int r = monitor_wait(mid, cvid);
  if (mon_verbose) printf("[mon] wait mid=%d cvid=%d -> %d\n", mid, cvid, r);
  return r;
}

uint64
sys_mon_signal(void)
{
  int mid, cvid;
  argint(0, &mid);
  argint(1, &cvid);
  int r = monitor_signal(mid, cvid);
  if (mon_verbose) printf("[mon] signal mid=%d cvid=%d -> %d\n", mid, cvid, r);
  return r;
}

uint64
sys_mon_broadcast(void)
{
  int mid, cvid;
  argint(0, &mid);
  argint(1, &cvid);
  int r = monitor_broadcast(mid, cvid);
  if (mon_verbose) printf("[mon] broadcast mid=%d cvid=%d -> %d\n", mid, cvid, r);
  return r;
}

// Enable/disable the deadlock detector (Phase B4).
// 1 = enabled, 0 = disabled.  Returns previous value.
extern int detector_enabled;
uint64
sys_deadlock_set(void)
{
  int on;
  argint(0, &on);
  int prev = detector_enabled;
  detector_enabled = (on != 0);
  return prev;
}

