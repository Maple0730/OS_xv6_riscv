#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "shm.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];
struct waitbucket waittable[NWCHAN];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;
uint64 mlfq_last_boost;

// Runtime scheduler switching support
volatile int current_scheduler = SCHED_MLFQ;  // 当前调度算法（默认 MLFQ）
struct spinlock sched_lock;                   // 保护调度器切换

// Runtime configurable timeslice values (ticks)
uint64 timeslice_table[MLFQ_LEVELS];  // MLFQ 各队列时间片
uint64 rr_fcfs_timeslice;            // RR/FCFS 共用时间片
struct spinlock timeslice_lock;      // 保护时间片配置

extern void forkret(void);
static void freeproc(struct proc *p, int wait_lock_held);
static struct waitbucket *waitbucket_for(void *chan);
static void waitlist_insert(struct waitbucket *wb, struct proc *p);
static void waitlist_remove(struct waitbucket *wb, struct proc *p);
static void mlfq_boost_priority(void);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  initlock(&sched_lock, "sched_lock");
  initlock(&timeslice_lock, "timeslice_lock");
  timeslice_table[0] = MLFQ_Q0_TIME;
  timeslice_table[1] = MLFQ_Q1_TIME;
  timeslice_table[2] = MLFQ_Q2_TIME;
  timeslice_table[3] = MLFQ_Q3_TIME;
  timeslice_table[4] = MLFQ_Q4_TIME;
  rr_fcfs_timeslice = TICKSLICE;
  for (int i = 0; i < NWCHAN; i++) {
    initlock(&waittable[i].lock, "waitbucket");
    waittable[i].head = 0;
  }
  mlfq_last_boost = 0;
  shminit();  // Initialize shared memory system
  for (p = proc; p < &proc[NPROC]; p++) {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    p->kstack = KSTACK((int)(p - proc));
    p->wnext = 0;
    p->wprev = 0;
    p->wbucket = 0;
    p->ctime = 0;
    p->queue_level = 0;
    p->timeslice_used = 0;
    p->last_sched = 0;
    p->priority = DEFAULT_PRIORITY;
    p->shm_shmidx = -1;
    p->wait_time = 0;
    p->run_time = 0;
    p->sched_count = 0;
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

static struct waitbucket *
waitbucket_for(void *chan)
{
  return &waittable[((uint64)chan) % NWCHAN];
}

static void
waitlist_insert(struct waitbucket *wb, struct proc *p)
{
  p->wnext = wb->head;
  p->wprev = 0;
  if (wb->head)
    wb->head->wprev = p;
  wb->head = p;
  p->wbucket = wb;
}

static void
waitlist_remove(struct waitbucket *wb, struct proc *p)
{
  if (p->wprev)
    p->wprev->wnext = p->wnext;
  else if (wb->head == p)
    wb->head = p->wnext;

  if (p->wnext)
    p->wnext->wprev = p->wprev;

  p->wnext = 0;
  p->wprev = 0;
  p->wbucket = 0;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  //进程表中寻找一个 UNUSED 的进程槽
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);//获取进程锁，确保独占访问
    if (p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  //初始化基本信息
  p->pid = allocpid();
  p->state = USED;//注意这里还没有变成 RUNNABLE，因为后面还有很多资源要准备。
  p->ctime = ticks;
  p->queue_level = 0;
  p->timeslice_used = 0;
  p->last_sched = 0;
  p->priority = DEFAULT_PRIORITY;
  p->cnext = 0;
  p->cprev = 0;
  p->child_count = 0;

  // 给 trapframe 分配内存
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0) {
    freeproc(p, 0);
    release(&p->lock);
    return 0;
  }

  // 创建该进程的用户页表
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0) {
    freeproc(p, 0);
    release(&p->lock);
    return 0;
  }

  // 初始化内核上下文 context
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
// wait_lock_held: 1 if caller holds wait_lock (kwait/kwaitpid path),
//                 0 otherwise (allocproc error path, p not in any child list).
static void
freeproc(struct proc *p, int wait_lock_held)
{
  // Remove from parent's child list.
  // wait_lock_held=1: caller holds wait_lock (kwait/kwaitpid path)
  // wait_lock_held=0: caller does not hold wait_lock (allocproc error path)
  //                  and p is not in any list, so list ops are no-ops
  if (wait_lock_held) {
    if (p->cprev) {
      p->cprev->cnext = p->cnext;
    } else if (p->parent) {
      p->parent->cnext = p->cnext;
    }
    if (p->cnext)
      p->cnext->cprev = p->cprev;
    if (p->parent)
      p->parent->child_count--;
  }

  if (p->trapframe)
    kfree((void *)p->trapframe);
  p->trapframe = 0;

  // Release shared memory mapping if this process has one.
  // We do NOT decrement refcount or call kfree here.
  // shmdt() is the ONLY place that manages refcount and frees pages.
  // This prevents double-free: freeproc never tries to kfree a shm page.
  p->shm_shmidx = -1;

  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->cnext = 0;
  p->cprev = 0;
  p->child_count = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->wnext = 0;
  p->wprev = 0;
  p->wbucket = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  p->ctime = 0;
  p->queue_level = 0;
  p->timeslice_used = 0;
  p->last_sched = 0;
  p->priority = DEFAULT_PRIORITY;
  p->shm_shmidx = -1;
  p->wait_time = 0;
  p->run_time = 0;
  p->sched_count = 0;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline,
               PTE_R | PTE_X) < 0) {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE, (uint64)(p->trapframe),
               PTE_R | PTE_W) < 0) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0) {
    if (sz + n > TRAPFRAME) {
      return -1;
    }
    if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if (n < 0) {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0) {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0) {
    freeproc(np, 0);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // Copy shared memory mappings from parent to child.
  // If parent has a shared memory page at SHM_BASE, map the same
  // physical page in the child (without copying data -- same page shared).
  // Also increment refcount to track the child's reference.
  pte_t *parent_pte = walk(p->pagetable, SHM_BASE, 0);
  if (parent_pte != 0 && (*parent_pte & PTE_V)) {
    uint64 pa = PTE2PA(*parent_pte);
    uint64 flags = PTE_FLAGS(*parent_pte);
    if (mappages(np->pagetable, SHM_BASE, SHM_SIZE, pa, flags) != 0) {
      freeproc(np, 0);
      release(&np->lock);
      return -1;
    }
    // Increment refcount for the child's reference
    acquire(&shm_lock);
    for (int si = 0; si < NSHM; si++) {
      if (shm_table[si].allocated && shm_table[si].phys_addr == pa) {
        shm_table[si].refcount++;
        break;
      }
    }
    release(&shm_lock);
  }

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  np->cprev = 0;
  np->cnext = p->cnext;
  if (p->cnext)
    p->cnext->cprev = np;
  p->cnext = np;
  p->child_count++;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  np->queue_level = 0;
  np->timeslice_used = 0;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *child = p->cnext;
  while (child) {
    struct proc *next = child->cnext;
    child->parent = initproc;
    // Detach from p's list
    if (child->cprev == p) {
      // child is the head of p's child list
      if (p->cnext == child)
        p->cnext = next;
    } else {
      // shouldn't happen in normal flow, but be safe
      if (child->cprev)
        child->cprev->cnext = next;
    }
    if (next)
      next->cprev = 0;
    child->cprev = 0;
    child->cnext = 0;
    // Attach to initproc's child list
    child->cnext = initproc->cnext;
    if (initproc->cnext)
      initproc->cnext->cprev = child;
    initproc->cnext = child;
    initproc->child_count++;
    wakeup(initproc);
    child = next;
  }
  p->child_count = 0;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
kexit(int status)
{
  struct proc *p = myproc();

  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++) {
    if (p->ofile[fd]) {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);

  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
kwait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;) {
    // Scan through table looking for exited children.
    havekids = 0;
    for (pp = proc; pp < &proc[NPROC]; pp++) {
      if (pp->parent == p) {
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if (pp->state == ZOMBIE) {
          // Found one.
          pid = pp->pid;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                   sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp, 1);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || killed(p)) {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock); //DOC: wait-sleep
  }
}

// Wait for a specific child process to exit.
// pid: specific PID to wait for, or -1 to wait for any child
// addr: address to store exit status
// Returns: pid of exited child, or -1 on error
int
kwaitpid(int pid, uint64 addr)
{
  struct proc *pp;
  int havekids;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;) {
    havekids = 0;

    // Scan through table looking for exited children.
    for (pp = proc; pp < &proc[NPROC]; pp++) {
      // Only consider children of this process
      if (pp->parent != p)
        continue;

      // If pid != -1, only consider the specific child
      if (pid != -1 && pp->pid != pid)
        continue;

      acquire(&pp->lock);
      havekids = 1;

      if (pp->state == ZOMBIE) {
        int ret_pid = pp->pid;
        // Copy exit status to user space
        if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                 sizeof(pp->xstate)) < 0) {
          release(&pp->lock);
          release(&wait_lock);
          return -1;
        }
        freeproc(pp, 1);
        release(&pp->lock);
        release(&wait_lock);
        return ret_pid;
      }
      release(&pp->lock);
    }

    // No point waiting if we don't have matching children.
    if (!havekids || killed(p)) {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock);
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  // Runtime scheduler switching - supports RR/FCFS/MLFQ without recompilation
  int algo = current_scheduler;
  if (algo == SCHED_FCFS) {
    fcfs_scheduler();
  } else if (algo == SCHED_MLFQ) {
    mlfq_scheduler();
  } else {
    rr_scheduler();
  }
}

// Get the name of the current scheduler for debugging
const char *
sched_algo_name(int algo)
{
  switch (algo) {
  case SCHED_FCFS:
    return "FCFS";
  case SCHED_MLFQ:
    return "MLFQ";
  default:
    return "RR";
  }
}

void
rr_scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for (;;) {
    intr_on();
    intr_off();

    int found = 0;
    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state == RUNNABLE) {
        p->state = RUNNING;
        p->last_sched = ticks;
        c->proc = p;
        swtch(&c->context, &p->context);
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if (found == 0) {
      asm volatile("wfi");
    }
  }
}

void
fcfs_scheduler(void)
{
  struct proc *p;
  struct proc *best;
  struct cpu *c = mycpu();
  uint64 min_ctime;

  c->proc = 0;
  for (;;) {
    intr_on();
    intr_off();

    min_ctime = (uint64)-1;
    best = 0;

    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state == RUNNABLE && p->ctime < min_ctime) {
        min_ctime = p->ctime;
        best = p;
      }
      release(&p->lock);
    }

    if (best) {
      acquire(&best->lock);
      if (best->state == RUNNABLE) {
        best->state = RUNNING;
        best->last_sched = ticks;
        c->proc = best;
        swtch(&c->context, &best->context);
        c->proc = 0;
      }
      release(&best->lock);
    } else {
      asm volatile("wfi");
    }
  }
}

int
get_timeslice(int queue_level)
{
  if (queue_level >= 0 && queue_level < MLFQ_LEVELS)
    return timeslice_table[queue_level];
  return timeslice_table[MLFQ_LEVELS - 1];
}

int
get_rr_fcfs_timeslice(void)
{
  return rr_fcfs_timeslice;
}

static void
mlfq_boost_priority(void)
{
  struct proc *p;

  // Note: we do NOT touch mlfq_lock here.
  // mlfq_enqueue/remove are informational only (not used for scheduling).
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == RUNNABLE || p->state == RUNNING) {
#if MLFQ_DEBUG
      printf("[MLFQ] boost: pid=%d from queue=%d to queue=0\n",
             p->pid, p->queue_level);
#endif
      p->queue_level = 0;
      p->timeslice_used = 0;
    }
    release(&p->lock);
  }
}

void
mlfq_scheduler(void)
{
  struct proc *p;
  struct proc *best;
  struct cpu *c = mycpu();
  int best_level;
  uint64 best_ctime;

  c->proc = 0;
  for (;;) {
    intr_on();
    intr_off();

    if (ticks - mlfq_last_boost >= MLFQ_BOOST_TICKS) {
      mlfq_boost_priority();
      mlfq_last_boost = ticks;
    }

    best = 0;
    best_level = MLFQ_LEVELS;
    best_ctime = (uint64)-1;

    // Scan proc table for the highest-priority RUNNABLE process.
    // (mlfq_enqueue/remove are not used for scheduling -- purely informational)
    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state == RUNNABLE &&
          (p->queue_level < best_level ||
           (p->queue_level == best_level && p->ctime < best_ctime))) {
        best = p;
        best_level = p->queue_level;
        best_ctime = p->ctime;
      }
      release(&p->lock);
    }

    if (best) {
      acquire(&best->lock);
      if (best->state == RUNNABLE) {
#if MLFQ_DEBUG
        printf("[MLFQ] schedule: pid=%d from queue=%d\n",
               best->pid, best->queue_level);
#endif
        uint64 now = ticks;
        if (best->last_sched != 0)
          best->wait_time += now - best->last_sched;
        best->sched_count++;
        best->state = RUNNING;
        best->last_sched = now;
        best->timeslice_used = 0;
        c->proc = best;
        swtch(&c->context, &best->context);
        c->proc = 0;
      }
      release(&best->lock);
    } else {
      asm volatile("wfi");
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched RUNNING");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  // Runtime MLFQ demotion check
  if (current_scheduler == SCHED_MLFQ) {
    if (p->timeslice_used >= get_timeslice(p->queue_level) &&
        p->queue_level < MLFQ_LEVELS - 1) {
#if MLFQ_DEBUG
      printf("[MLFQ] demote(yield): pid=%d from queue=%d to queue=%d\n",
             p->pid, p->queue_level, p->queue_level + 1);
#endif
      p->queue_level++;
    }
    p->timeslice_used = 0;
  }
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  extern char userret[];
  static int first = 1;
  struct proc *p = myproc();

  // Still holding p->lock from scheduler.
  release(&p->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    // We can invoke kexec() now that file system is initialized.
    // Put the return value (argc) of kexec into a0.
    p->trapframe->a0 = kexec("/init", (char *[]){"/init", 0});
    if (p->trapframe->a0 == -1) {
      panic("exec");
    }
  }

  // return to user space, mimicing usertrap()'s return.
  prepare_return();
  uint64 satp = MAKE_SATP(p->pagetable);
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// Sleep on channel chan, releasing condition lock lk.
// Re-acquires lk when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  struct waitbucket *wb = waitbucket_for(chan);

  acquire(&wb->lock);
  acquire(&p->lock); //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  waitlist_insert(wb, p);
  p->state = SLEEPING;

  release(&wb->lock);
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on channel chan.
// Caller should hold the condition lock.
void
wakeup(void *chan)
{
  struct waitbucket *wb = waitbucket_for(chan);
  struct proc *p;
  struct proc *next;

  acquire(&wb->lock);
  for (p = wb->head; p != 0; p = next) {
    next = p->wnext;
    if (p != myproc()) {
      acquire(&p->lock);
      if (p->state == SLEEPING && p->chan == chan) {
        waitlist_remove(wb, p);
        p->state = RUNNABLE;
        p->timeslice_used = 0;
      }
      release(&p->lock);
    }
  }
  release(&wb->lock);
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kkill(int pid)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    struct waitbucket *wb;

    acquire(&p->lock);
    if (p->pid != pid) {
      release(&p->lock);
      continue;
    }

    p->killed = 1;
    wb = p->wbucket;
    release(&p->lock);

    if (wb)
      acquire(&wb->lock);
    acquire(&p->lock);
    if (p->pid != pid) {
      release(&p->lock);
      if (wb)
        release(&wb->lock);
      continue;
    }

    p->killed = 1;
    if (p->state == SLEEPING) {
      if (wb && p->wbucket == wb)
        waitlist_remove(wb, p);
      p->state = RUNNABLE;
      p->timeslice_used = 0;
    }
    release(&p->lock);
    if (wb)
      release(&wb->lock);
    return 0;
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst) {
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src) {
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
    // clang-format off
    [UNUSED]    "unused",
    [USED]      "used",
    [SLEEPING]  "sleep ",
    [RUNNABLE]  "runble",
    [RUNNING]   "run   ",
    [ZOMBIE]    "zombie"
    // clang-format on
  };
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++) {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
