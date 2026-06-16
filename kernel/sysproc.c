#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

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
// arg0: algo (0=RR, 1=FCFS, 2=MLFQ, -1=query only)
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

  // Validate algorithm number
  if (algo < 0 || algo > 2)
    return -1;

  acquire(&sched_lock);
  int old = current_scheduler;
  current_scheduler = algo;
  release(&sched_lock);

  return old;
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
