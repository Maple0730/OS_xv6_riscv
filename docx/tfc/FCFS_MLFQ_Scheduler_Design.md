# FCFS 与 MLFQ 调度算法设计方案

## 文档概述

本文档详细设计 xv6-riscv 操作系统的两种调度算法：
1. **FCFS（先来先服务）调度算法** - 实现简单、无抢占的调度策略
2. **MLFQ（多级反馈队列）调度算法** - 支持优先级和时间片动态调整的调度策略

**项目路径**：`/home/tfc/OS/OS_xv6_riscv`
**文档路径**：`/docx/tfc/FCFS_MLFQ_Scheduler_Design.md`
**最后更新**：2026年6月10日

---

## 1. 背景与当前实现分析

### 1.1 当前调度器实现

当前 `kernel/proc.c:471-510` 中的 `scheduler()` 实现的是简单的 RR（时间片轮转）调度：

```c
void scheduler(void) {
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
```

**当前限制**：
- 进程按 proc 数组顺序遍历，非 FIFO 顺序
- 时间片硬编码为 1000000 ticks（约10ms），不可配置
- 无优先级支持
- 无抢占机制（仅依赖时钟中断 yield）

### 1.2 现有进程结构分析

```c
// kernel/proc.h:89-115
struct proc {
  struct spinlock lock;
  enum procstate state;
  void *chan;
  int killed;
  int xstate;
  int pid;
  struct proc *wnext, *wprev;
  struct waitbucket *wbucket;
  struct proc *parent;
  uint64 kstack;
  uint64 sz;
  pagetable_t pagetable;
  struct trapframe *trapframe;
  struct context context;
  struct file *ofile[NOFILE];
  struct inode *cwd;
  char name[16];
};
```

---

## 2. FCFS 调度算法设计

### 2.1 算法原理

**FCFS（First Come First Serve）** 是最简单的调度算法：
- 按照进程创建时间（PID 顺序）选择最早创建的 RUNNABLE 进程
- 非抢占式调度，进程运行直到主动让出 CPU
- 优点：实现简单、无饥饿问题
- 缺点：短作业可能等待长作业（ convoy effect）

### 2.2 需要新增的数据结构

#### 2.2.1 进程创建时间戳

在 `kernel/proc.h` 的 `struct proc` 中新增字段：

```c
uint64 ctime;  // 进程创建时间（ticks）
```

#### 2.2.2 调度算法选择器

在 `kernel/param.h` 中新增：

```c
// 调度算法选择
#define SCHED_RR      0  // 时间片轮转（默认）
#define SCHED_FCFS    1  // 先来先服务
#define SCHED_MLFQ    2  // 多级反馈队列

#ifndef SCHED_ALGORITHM
#define SCHED_ALGORITHM SCHED_RR  // 默认使用 RR
#endif
```

### 2.3 FCFS 调度器实现

#### 2.3.1 修改 `allocproc()` 记录创建时间

```c
// kernel/proc.c - allocproc() 中添加
found:
  p->pid = allocpid();
  p->state = USED;
  p->ctime = ticks;  // 新增：记录创建时间
  // ... 其余代码不变
```

#### 2.3.2 实现 FCFS 调度器

```c
// kernel/proc.c - 新增函数
void
fcfs_scheduler(void)
{
  struct proc *p;
  struct proc *best = 0;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  
  for (;;) {
    intr_on();
    intr_off();
    
    // 找到 ctime 最小的 RUNNABLE 进程
    uint64 min_ctime = UINT64_MAX;
    best = 0;
    
    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state == RUNNABLE) {
        if (p->ctime < min_ctime) {
          min_ctime = p->ctime;
          best = p;
        }
      }
      release(&p->lock);
    }
    
    if (best) {
      acquire(&best->lock);
      if (best->state == RUNNABLE) {
        best->state = RUNNING;
        c->proc = best;
        swtch(&c->context, &best->context);
        c->proc = 0;
      }
      release(&best->lock);
    } else {
      // 无可运行进程，等待
      asm volatile("wfi");
    }
  }
}
```

#### 2.3.3 统一调度器入口

```c
// kernel/proc.c - 修改 scheduler()
void
scheduler(void)
{
#if SCHED_ALGORITHM == SCHED_FCFS
  fcfs_scheduler();
#elif SCHED_ALGORITHM == SCHED_MLFQ
  mlfq_scheduler();
#else
  // 默认 RR 调度
  rr_scheduler();
#endif
}
```

### 2.4 FCFS 关键设计决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| 时间复杂度 | O(NPROC) | 需要遍历进程表找最小 ctime |
| 锁粒度 | 单进程锁 | 保持与现有代码一致 |
| 时间片 | 无（非抢占） | FCFS 特性 |
| 饥饿问题 | 无 | 公平按创建顺序 |

---

## 3. MLFQ 调度算法设计

### 3.1 算法原理

**MLFQ（Multilevel Feedback Queue）** 是一种动态优先级调度算法：
- 维护多个优先级队列，从高到低排列
- 高优先级队列使用较小时间片（快速响应交互式进程）
- 低优先级队列使用较大时间片（充分利用 CPU）
- 进程在队列间可以降级（用完时间片）或提升（等待过久）

### 3.2 队列设计

采用 **3 级队列** 设计：

| 队列 | 优先级 | 时间片 | 适用场景 |
|------|--------|--------|----------|
| Queue 0 | 最高 | 5ms | 交互式进程、短作业 |
| Queue 1 | 中等 | 10ms | 普通进程 |
| Queue 2 | 最低 | 20ms | CPU 密集型进程 |

### 3.3 需要新增的数据结构

#### 3.3.1 队列优先级和时间片定义

在 `kernel/param.h` 中新增：

```c
// MLFQ 调度参数
#define MLFQ_LEVELS      3       // 队列层数
#define MLFQ_Q0_TIME     500000   // Queue 0 时间片（5ms）
#define MLFQ_Q1_TIME     1000000  // Queue 1 时间片（10ms）
#define MLFQ_Q2_TIME     2000000  // Queue 2 时间片（20ms）
#define MLFQ_BOOST_TICKS 100     // 每 100 ticks 提升一次优先级
```

#### 3.3.2 进程结构扩展

在 `kernel/proc.h` 的 `struct proc` 中新增：

```c
int queue_level;     // 当前所在队列级别 (0, 1, 2)
int timeslice_used;  // 本时间片内已用时间
uint64 last_sched;   // 上次被调度的时间（用于优先级提升）
```

#### 3.3.3 进程运行时间统计（可选，用于调试）

```c
uint64 runtime;     // 累计运行时间（用于统计）
```

### 3.4 MLFQ 调度器实现

#### 3.4.1 队列初始化

```c
// kernel/proc.c - procinit() 中新增
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  
  // 初始化等待桶（已有代码）
  for (int i = 0; i < NWCHAN; i++) {
    initlock(&waittable[i].lock, "waitbucket");
    waittable[i].head = 0;
  }
  
  // 初始化 MLFQ 队列锁
  for (int i = 0; i < MLFQ_LEVELS; i++) {
    initlock(&mlfq_lock[i], "mlfq");
    mlfq_head[i] = 0;
    mlfq_tail[i] = 0;
  }
  
  // 初始化进程
  for (p = proc; p < &proc[NPROC]; p++) {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    p->kstack = KSTACK((int)(p - proc));
    p->wnext = 0;
    p->wprev = 0;
    p->wbucket = 0;
    p->queue_level = 0;      // 默认最高优先级
    p->timeslice_used = 0;
    p->last_sched = 0;
    p->ctime = 0;
  }
}
```

#### 3.4.2 MLFQ 调度器核心

```c
// kernel/proc.c - 新增函数
struct spinlock mlfq_lock[MLFQ_LEVELS];
struct proc *mlfq_head[MLFQ_LEVELS];
struct proc *mlfq_tail[MLFQ_LEVELS];

// 获取指定队列的时间片
static int
get_timeslice(int queue_level)
{
  switch (queue_level) {
    case 0: return MLFQ_Q0_TIME;
    case 1: return MLFQ_Q1_TIME;
    case 2: return MLFQ_Q2_TIME;
    default: return MLFQ_Q2_TIME;
  }
}

// 将进程加入 MLFQ 队列
static void
mlfq_enqueue(struct proc *p)
{
  int ql = p->queue_level;
  if (ql < 0 || ql >= MLFQ_LEVELS)
    ql = 0;
  
  acquire(&mlfq_lock[ql]);
  
  p->wnext = 0;
  if (mlfq_tail[ql]) {
    mlfq_tail[ql]->wnext = p;
    p->wprev = mlfq_tail[ql];
    mlfq_tail[ql] = p;
  } else {
    mlfq_head[ql] = mlfq_tail[ql] = p;
    p->wprev = 0;
  }
  
  release(&mlfq_lock[ql]);
}

// 从 MLFQ 队列取出进程
static struct proc*
mlfq_dequeue(void)
{
  struct proc *p = 0;
  
  // 从高优先级队列开始找
  for (int ql = 0; ql < MLFQ_LEVELS; ql++) {
    acquire(&mlfq_lock[ql]);
    
    if (mlfq_head[ql]) {
      p = mlfq_head[ql];
      mlfq_head[ql] = p->wnext;
      if (mlfq_head[ql] == 0)
        mlfq_tail[ql] = 0;
      else
        mlfq_head[ql]->wprev = 0;
      p->wnext = 0;
      p->wprev = 0;
    }
    
    release(&mlfq_lock[ql]);
    
    if (p)
      break;
  }
  
  return p;
}

// MLFQ 主调度器
void
mlfq_scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  uint64 last_boost = 0;
  
  c->proc = 0;
  
  for (;;) {
    intr_on();
    intr_off();
    
    // 优先级提升：防止低优先级进程饥饿
    if (ticks - last_boost >= MLFQ_BOOST_TICKS) {
      mlfq_boost_priority();
      last_boost = ticks;
    }
    
    // 从队列中取出进程
    p = mlfq_dequeue();
    
    if (p) {
      acquire(&p->lock);
      if (p->state == RUNNABLE) {
        p->state = RUNNING;
        p->last_sched = ticks;
        p->timeslice_used = 0;
        c->proc = p;
        swtch(&c->context, &p->context);
        c->proc = 0;
      }
      release(&p->lock);
    } else {
      // 无可运行进程，等待
      asm volatile("wfi");
    }
  }
}

// 提升所有进程优先级
static void
mlfq_boost_priority(void)
{
  struct proc *p;
  
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == RUNNABLE || p->state == RUNNING) {
      if (p->queue_level > 0) {
        p->queue_level--;  // 提升一级
      }
    }
    release(&p->lock);
  }
}
```

#### 3.4.3 时间片用完处理

修改 `kernel/trap.c` 中的时钟中断处理：

```c
// kernel/trap.c - 修改 usertrap()
void
usertrap(void)
{
  // ... 现有代码 ...
  
  if (killed(p))
    kexit(-1);
  
  // 时钟中断处理
  if (which_dev == 2) {
#if SCHED_ALGORITHM == SCHED_MLFQ
    // MLFQ 模式：检查是否用完时间片
    p->timeslice_used++;
    int ts = get_timeslice(p->queue_level);
    if (p->timeslice_used >= ts) {
      // 时间片用完，降级到低优先级队列
      if (p->queue_level < MLFQ_LEVELS - 1) {
        p->queue_level++;
      }
      p->timeslice_used = 0;
      yield();
    }
#else
    yield();  // RR/FCFS 模式直接 yield
#endif
  }
  
  // ... 其余代码 ...
}
```

#### 3.4.4 睡眠/唤醒与队列管理

修改 `kernel/proc.c` 中的 `sleep()` 和 `wakeup()`：

```c
// sleep - 进程进入睡眠时不需要特殊处理（保持在原队列）
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  struct waitbucket *wb = waitbucket_for(chan);
  
  acquire(&wb->lock);
  acquire(&p->lock);
  release(lk);
  
  p->chan = chan;
  waitlist_insert(wb, p);
  p->state = SLEEPING;
  
  release(&wb->lock);
  sched();
  
  p->chan = 0;
  release(&p->lock);
  acquire(lk);
}

// wakeup - 被唤醒的进程重新加入队列
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
        
#if SCHED_ALGORITHM == SCHED_MLFQ
        // MLFQ: 重新入队
        mlfq_enqueue(p);
#endif
      }
      release(&p->lock);
    }
  }
  release(&wb->lock);
}

// yield - 主动让出时根据情况决定是否降级
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  
#if SCHED_ALGORITHM == SCHED_MLFQ
  // MLFQ: 如果是时间片用完（timeslice_used > 0）才降级
  // 如果是主动让出（如 sleep），不降级
  if (p->timeslice_used > 0 && p->queue_level < MLFQ_LEVELS - 1) {
    p->queue_level++;  // 降级
  }
  p->timeslice_used = 0;
#endif
  
  p->state = RUNNABLE;
  
#if SCHED_ALGORITHM == SCHED_MLFQ
  mlfq_enqueue(p);  // 重新入队
#endif
  
  sched();
  release(&p->lock);
}
```

### 3.5 MLFQ 关键设计决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| 队列层数 | 3 | 足够表现优先级差异，实现复杂度适中 |
| 时间片大小 | 5/10/20ms | 兼顾交互性和吞吐量 |
| 优先级提升周期 | 100 ticks | 防止低优先级进程饥饿 |
| 降级策略 | 用完时间片降级 | 区分 CPU 密集型和交互式进程 |
| 锁设计 | 每队列独立锁 | 减少锁竞争 |

---

## 4. 验证测试方案

### 4.1 测试框架设计

#### 4.1.1 测试用户程序

创建 `user/test_sched.c`：

```c
#include "kernel/types.h"
#include "user/user.h"

// 测试参数
#define NUM_PROCESSES 8
#define RUN_TIME 100  // 每个进程运行时间单位

// 记录进程执行顺序和时间
struct log_entry {
  int pid;
  int action;  // 0=start, 1=yield, 2=exit
  int ticks;
};

volatile struct log_entry logs[256];
volatile int log_idx = 0;

void test_fork(int id) {
  int pid = fork();
  if (pid == 0) {
    // 子进程
    printf("Process %d (pid=%d) started\n", id, getpid());
    
    for (int i = 0; i < RUN_TIME; i++) {
      // 模拟工作
      if (i % 10 == 0) {
        printf("Process %d (pid=%d) at tick %d\n", id, getpid(), uptime());
      }
      // 主动让出 CPU（测试调度器）
      sleep(0);
    }
    
    printf("Process %d (pid=%d) exiting\n", id, getpid());
    exit(0);
  }
}

int main(int argc, char *argv[]) {
  printf("Scheduling Test\n");
  printf("Current time: %d\n", uptime());
  
  // 创建多个进程
  for (int i = 0; i < NUM_PROCESSES; i++) {
    test_fork(i);
  }
  
  // 父进程等待所有子进程
  for (int i = 0; i < NUM_PROCESSES; i++) {
    wait(0);
  }
  
  printf("All processes finished\n");
  exit(0);
}
```

#### 4.1.2 MLFQ 优先级测试

创建 `user/test_mlfq.c`：

```c
#include "kernel/types.h"
#include "user/user.h"

#define SHORT_WORK  20
#define LONG_WORK   100
#define ITERATIONS  5

// 短作业进程 - 应该在高优先级队列
void short_job(int id) {
  printf("Short job %d (pid=%d) started\n", id, getpid());
  for (int i = 0; i < SHORT_WORK; i++) {
    // 短暂工作后让出
  }
  printf("Short job %d (pid=%d) finished\n", id, getpid());
  exit(0);
}

// 长作业进程 - 应该在低优先级队列
void long_job(int id) {
  printf("Long job %d (pid=%d) started\n", id, getpid());
  for (int i = 0; i < LONG_WORK; i++) {
    // 长时间工作
  }
  printf("Long job %d (pid=%d) finished\n", id, getpid());
  exit(0);
}

// 混合型进程
void mixed_job(int id) {
  printf("Mixed job %d (pid=%d) started\n", id, getpid());
  for (int iter = 0; iter < ITERATIONS; iter++) {
    for (volatile int i = 0; i < 30; i++);  // 工作
  }
  printf("Mixed job %d (pid=%d) finished\n", id, getpid());
  exit(0);
}

int main() {
  printf("MLFQ Priority Test\n");
  
  // 创建短作业
  for (int i = 0; i < 2; i++) {
    if (fork() == 0) short_job(i);
  }
  
  // 创建长作业
  for (int i = 0; i < 2; i++) {
    if (fork() == 0) long_job(i);
  }
  
  // 创建混合作业
  for (int i = 0; i < 2; i++) {
    if (fork() == 0) mixed_job(i);
  }
  
  // 等待所有
  for (int i = 0; i < 6; i++) {
    wait(0);
  }
  
  printf("MLFQ Test Complete\n");
  return 0;
}
```

### 4.2 性能指标测试

#### 4.2.1 吞吐量测试

```c
// user/throughput_test.c
#include "user/user.h"

#define NUM_PROCS 16
#define WORK_UNITS 1000

int main() {
  int start = uptime();
  int pids[NUM_PROCS];
  
  // 创建进程
  for (int i = 0; i < NUM_PROCS; i++) {
    pids[i] = fork();
    if (pids[i] == 0) {
      // 执行计算密集型任务
      volatile long sum = 0;
      for (int j = 0; j < WORK_UNITS * 1000; j++) {
        sum += j;
      }
      exit(0);
    }
  }
  
  // 等待所有进程
  for (int i = 0; i < NUM_PROCS; i++) {
    wait(0);
  }
  
  int elapsed = uptime() - start;
  printf("Throughput test: %d processes completed in %d ticks\n", 
         NUM_PROCS, elapsed);
  printf("Average time per process: %d ticks\n", elapsed / NUM_PROCS);
  
  return 0;
}
```

#### 4.2.2 响应时间测试

```c
// user/response_test.c
#include "user/user.h.h"

#define NUM_SPAWN 5
#define WORK_TIME 5

void interactive_process(int id) {
  int spawn_time = uptime();
  printf("Interactive %d spawned at %d\n", id, spawn_time);
  
  // 短暂工作后等待（模拟用户输入）
  for (volatile int i = 0; i < WORK_TIME; i++);
  
  int response_time = uptime() - spawn_time;
  printf("Interactive %d response time: %d ticks\n", id, response_time);
  exit(0);
}

int main() {
  printf("Response Time Test\n");
  
  for (int i = 0; i < NUM_SPAWN; i++) {
    int pid = fork();
    if (pid == 0) {
      interactive_process(i);
    }
    // 父进程短暂等待后创建下一个（确保有时间间隔）
    sleep(1);
  }
  
  while (wait(0) != -1);
  return 0;
}
```

#### 4.2.3 上下文切换开销测试

```c
// user/context_switch_test.c
#include "user/user.h.h"

#define YIELDS_PER_CHILD 1000
#define NUM_CHILDREN 4

volatile int ready = 0;
volatile int done_count = 0;

void yield_process(int id) {
  // 等待所有进程准备好
  __sync_fetch_and_add(&ready, 1);
  while (ready < NUM_CHILDREN + 1);  // +1 for parent
  
  for (int i = 0; i < YIELDS_PER_CHILD; i++) {
    yield();  // 系统调用
  }
  
  __sync_fetch_and_add(&done_count, 1);
  exit(0);
}

int main() {
  printf("Context Switch Test\n");
  
  int start = uptime();
  
  // 创建子进程
  for (int i = 0; i < NUM_CHILDREN; i++) {
    if (fork() == 0) {
      yield_process(i);
    }
  }
  
  // 等待所有进程准备好
  while (ready < NUM_CHILDREN);
  
  int work_start = uptime();
  
  // 父进程也进行 yield
  for (int i = 0; i < YIELDS_PER_CHILD; i++) {
    yield();
  }
  
  // 等待子进程
  while (done_count < NUM_CHILDREN);
  
  int total = uptime() - work_start;
  
  printf("Total time for %d processes * %d yields: %d ticks\n",
         NUM_CHILDREN + 1, YIELDS_PER_CHILD, total);
  printf("Average context switch time: ~%d ticks\n", 
         total / ((NUM_CHILDREN + 1) * YIELDS_PER_CHILD));
  
  return 0;
}
```

### 4.3 测试场景设计

| 测试名称 | 测试目的 | 验证指标 | 预期结果 |
|----------|----------|----------|----------|
| FCFS 顺序 | 验证 FCFS 按创建顺序调度 | 进程完成顺序 | 应与 PID 顺序一致 |
| MLFQ 优先级 | 验证短作业优先 | 短作业周转时间 | 短作业应更快完成 |
| MLFQ 降级 | 验证 CPU 密集型降级 | 低优先级队列使用率 | CPU 密集型应在低队列 |
| 饥饿防止 | 验证优先级提升 | 最低队列进程延迟 | 长时间等待后应提升 |
| 吞吐量 | 测量系统吞吐 | 完成时间 | 与 RR 对比应相近或更好 |
| 上下文切换 | 测量切换开销 | 平均切换时间 | 应 < 1ms |

### 4.4 对比验证框架

创建 `user/sched_compare.c` 支持不同调度算法对比：

```c
// user/sched_compare.c
#include "user/user.h.h"

#define NUM_PROCS 8
#define WORK_UNITS 500

void cpu_bound(int id) {
  volatile long sum = 0;
  for (int i = 0; i < WORK_UNITS * 1000; i++) {
    sum += i;
  }
  exit(id);
}

void io_bound(int id) {
  for (int i = 0; i < WORK_UNITS; i++) {
    sleep(1);  // 模拟 I/O 等待
  }
  exit(id);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: sched_compare <algorithm>\n");
    printf("  0 = RR, 1 = FCFS, 2 = MLFQ\n");
    exit(1);
  }
  
  int algo = atoi(argv[1]);
  printf("Running with algorithm: %s\n", 
         algo == 0 ? "RR" : (algo == 1 ? "FCFS" : "MLFQ"));
  
  int start = uptime();
  
  // 创建混合类型进程
  for (int i = 0; i < NUM_PROCS; i++) {
    if (fork() == 0) {
      if (i < NUM_PROCS / 2) {
        cpu_bound(i);
      } else {
        io_bound(i);
      }
    }
  }
  
  // 等待
  for (int i = 0; i < NUM_PROCS; i++) {
    wait(0);
  }
  
  int elapsed = uptime() - start;
  printf("Total time: %d ticks\n", elapsed);
  
  return 0;
}
```

---

## 5. 实现计划

### Phase 1: FCFS 调度器（预计工时：1天）

1. **修改 `kernel/proc.h`**
   - 添加 `ctime` 字段

2. **修改 `kernel/proc.c`**
   - `allocproc()` 初始化 `ctime`
   - 实现 `fcfs_scheduler()`
   - 修改 `scheduler()` 支持算法选择

3. **修改 `kernel/param.h`**
   - 添加 `SCHED_ALGORITHM` 宏

4. **测试验证**
   - 编写 `user/test_fcfs.c`
   - 验证调度顺序

### Phase 2: MLFQ 调度器基础（预计工时：2天）

1. **修改 `kernel/proc.h`**
   - 添加 `queue_level`、`timeslice_used`、`last_sched` 字段

2. **修改 `kernel/proc.c`**
   - 添加 MLFQ 队列管理函数
   - 实现 `mlfq_scheduler()`
   - 修改 `yield()` 支持队列操作

3. **修改 `kernel/trap.c`**
   - 修改时钟中断处理
   - 实现时间片用完检测

4. **测试验证**
   - 编写 `user/test_mlfq.c`
   - 验证优先级行为

### Phase 3: MLFQ 优化与饥饿防止（预计工时：1天）

1. **实现优先级提升**
   - 添加 boost 定时器
   - 实现 `mlfq_boost_priority()`

2. **性能测试**
   - 吞吐量测试
   - 响应时间测试

### Phase 4: 完整测试套件（预计工时：1天）

1. **测试框架完善**
   - 上下文切换开销测试
   - 调度算法对比工具

2. **压力测试**
   - 多进程并发测试
   - 边界条件测试

---

## 6. 风险与注意事项

### 6.1 潜在风险

| 风险项 | 描述 | 缓解措施 |
|--------|------|----------|
| 锁竞争 | MLFQ 多队列访问可能增加锁竞争 | 每队列独立锁，减少竞争 |
| 饥饿 | 低优先级进程可能长期得不到调度 | 优先级提升机制 |
| 代码侵入性 | 修改调度器可能影响现有功能 | 充分测试，保持兼容性 |
| 时间片精度 | ticks 精度可能不足 | 使用更精细的计时器 |

### 6.2 兼容性考虑

- 所有修改保持现有 API 不变
- 调度算法通过编译时宏选择
- 睡眠/唤醒机制不受影响
- 上下文切换路径保持兼容

---

## 7. 附录：文件修改清单

### kernel/proc.h
```diff
 struct proc {
   // ... 现有字段 ...
+  int queue_level;     // MLFQ 队列级别
+  int timeslice_used;  // 已用时间片
+  uint64 last_sched;   // 上次调度时间
+  uint64 ctime;        // 创建时间
 };
```

### kernel/proc.c
- `procinit()`: 初始化 MLFQ 队列
- `allocproc()`: 初始化 `ctime`、`queue_level`
- `scheduler()`: 添加算法选择
- `yield()`: MLFQ 模式重入队
- `sleep()`: 保持原队列状态
- `wakeup()`: MLFQ 模式重入队
- 新增 `fcfs_scheduler()`
- 新增 `mlfq_scheduler()`
- 新增 `mlfq_enqueue()`/`mlfq_dequeue()`
- 新增 `mlfq_boost_priority()`

### kernel/trap.c
```diff
 if (which_dev == 2) {
+ #if SCHED_ALGORITHM == SCHED_MLFQ
+   // MLFQ 时间片检查
+ #else
   yield();
+ #endif
 }
```

### kernel/param.h
```diff
+ // 调度算法选择
+ #define SCHED_RR      0
+ #define SCHED_FCFS    1
+ #define SCHED_MLFQ    2
+ #define SCHED_ALGORITHM SCHED_RR
+
+ // MLFQ 参数
+ #define MLFQ_LEVELS      3
+ #define MLFQ_Q0_TIME     500000
+ #define MLFQ_Q1_TIME     1000000
+ #define MLFQ_Q2_TIME     2000000
+ #define MLFQ_BOOST_TICKS 100
```

---

**文档结束**
