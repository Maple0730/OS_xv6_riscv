# 进程管理 & 处理机调度 — 高级扩展路线图

> 文档定位：在已完成的基础板块（PCB/状态/fork-exec-wait/信号量/共享内存/RR/FCFS/SJF/MLFQ/运行时切换/动态时间片/调度统计/高精度计时）之上，补齐 OS 课程"进程管理 + 处理机调度"一章在 **理论完整性 + 汇报演示效果** 两个维度上仍缺的关键板块。
>
> 适用对象：xv6-riscv 内核扩展 / 操作系统课程设计
> 总体方法：**理论体系完整 + 汇报效果突出 + 内核态真实现**
> 评估日期：2026-06-16
> 评估方法：源码实测（`kernel/proc.c`、`kernel/sem.c`、`kernel/sysproc.c`、`user/sjftest.c`）+ `Done.md` 交叉验证
> 关联文档：[`ProcessMgmt_Scheduling_NextPhase.md`](ProcessMgmt_Scheduling_NextPhase.md)（早一版规划） / [`FCFS_MLFQ_Scheduler_Design.md`](FCFS_MLFQ_Scheduler_Design.md)（调度器设计） / [`Doing.md`](Doing.md)（进行中） / [`Todo.md`](Todo.md)（任务清单） / [`Done.md`](Done.md)（已完成）

---

## 0. 总体逻辑脉络

按经典操作系统教材（汤小丹 / 操作系统概念 / OSTEP）"进程管理 + 处理机调度"的标准结构，把已完成与待扩展的内容组织成五层递进：

```
L1 进程基础   (PCB / 状态 / fork-exec-wait)          ✅ 已完成
   ↓
L2 同步互斥   (信号量 / 管程)                         ✅ 信号量；🔜 管程 + 条件变量
   ↓
L3 死锁与处理 (4 条件 / 预防 / 银行家 / 检测)           🔜 核心扩展板块
   ↓
L4 经典调度   (FCFS / SJF / RR / MLFQ)                ✅ 已完成
   ↓
L5 调度扩展   (优先级/实时/多核)                       ⚠️ 部分（priority 字段有但未调度）；🔜 大幅扩展
```

**逻辑主线**：
- L1 解决"进程是什么、怎么管" → 已有 PCB、状态转换、fork/exec/wait
- L2 解决"进程怎么合作" → 已有信号量，**需补管程**（与信号量并列的高级范式）
- L3 解决"合作出了问题怎么办" → **死锁的 4 条件 + 4 处理方法**（教学核心）
- L4 解决"CPU 怎么分配" → 已有 RR/FCFS/SJF/MLFQ；**需补优先级调度 + 优先级继承**
- L5 解决"更复杂场景下怎么调度" → **实时（RM/EDF）+ 多核（Per-CPU 队列 / 负载均衡）**

---

## 1. 现状评估（基于源码实测）

### 1.1 已完成板块清单

| 知识板块 | 已实现能力 | 关键文件 / 验证 |
|----------|------------|----------------|
| 进程控制 | fork / exec / wait / waitpid / exit / kill | `kernel/proc.c` `kfork`/`kwait`/`kwaitpid`；`user/waitpidtest.c` |
| 进程状态 | UNUSED/USED/SLEEPING/RUNNABLE/RUNNING/ZOMBIE | `kernel/proc.h` `enum procstate`；`user/semtest*.c` |
| PCB 管理 | 双向子进程链表 + waitbucket 哈希等待队列 | `kernel/proc.c` `cnext/cprev` + `waitbucket_for` |
| 同步互斥 | 信号量（sem_open/wait/post/get/close）+ sem_waiter 等待队列 | `kernel/sem.c`、`kernel/sem.h`；`user/semtest1/2/3.c` |
| IPC | 共享内存（shmget/shmat/shmdt + 引用计数 + fork 继承） | `kernel/shm.c`；`user/shmtest.c` |
| 调度算法 | RR / FCFS / SJF（非抢占）/ MLFQ 5 级队列 | `kernel/proc.c` `scheduler()`；`kernel/param.h` SCHED_RR..SCHED_SJF |
| 调度增强 | 运行时切换（#34）、动态时间片（#35-36）、调度统计（#38）、高精度计时（#37） | `kernel/sysproc.c` `sys_sched_algorithm`/`sys_settimeslice`/`sys_schedstat`/`sys_cgettimeofday` |
| 性能测试 | throughput、schedlatency、sjfbusy（已验证 est=1,2,4,8 完成顺序） | `user/throughput.c`、`user/schedlatency.c`、`user/sjfbusy.c` |

### 1.2 OS 课程标准板块覆盖度（理论完整性视角）

对照汤小丹 / OSTEP / 操作系统概念 经典结构：

| 知识板块 | 课程重要性 | 当前状态 | 缺口 |
|----------|------------|----------|------|
| 进程概念 / 状态 / PCB | ★★★★★ | ✅ | — |
| 进程控制 fork/exec/wait | ★★★★★ | ✅ | — |
| 同步：信号量 | ★★★★★ | ✅ | — |
| **同步：管程 + 条件变量** | ★★★★★ | ❌ | 缺 Phase C1-C2 |
| **死锁 4 条件 + 复现** | ★★★★★ | ❌ | 缺 Phase B1 |
| **死锁预防** | ★★★★ | ❌ | 缺 Phase B2 |
| **死锁避免（银行家）** | ★★★★★ | ❌ | 缺 Phase B3 |
| **死锁检测 + 恢复** | ★★★★ | ❌ | 缺 Phase B4 |
| IPC：共享内存 | ★★★★ | ✅ | — |
| **IPC：消息队列** | ★★★ | ❌ | 缺 Phase D2 |
| 调度：FCFS | ★★★★ | ✅ | — |
| 调度：SJF | ★★★★ | ✅ | — |
| **调度：优先级（非 MLFQ）** | ★★★ | ⚠️ 字段有未调度 | 缺 Phase A2 |
| 调度：RR | ★★★★ | ✅ | — |
| 调度：MLFQ | ★★★★★ | ✅ | — |
| **优先级反转 / 继承** | ★★★★★ | ❌ | 缺 Phase A2 + D1 |
| **实时调度（RM/EDF）** | ★★★ | ❌ | 缺 Phase F1-F3 |
| **多核调度** | ★★★★ | ❌ | 缺 Phase E1-E3 |

### 1.3 与现有成果的衔接点

- 复用 `struct proc.priority`（已存在但未用于调度决策）
- 复用 `sem_open/sem_wait/sem_post`（哲学家就餐、银行家并发请求）
- 复用 `sys_sched_algorithm()`（Phase A2 通过 `sys_sched_algorithm(4)` 扩展算法编号即可）
- 复用 `sys_schedstat()`（B3 / B4 / E2 扩展统计字段）
- 复用 `cgettimeofday()`（E2 负载均衡、F3 实时任务截止时间达成率）
- 复用 `sjfbusy` 的 sem-barrier 模式（B1 哲学家死锁可参考）

---

## 2. 高级扩展板块（按"汇报价值 × 理论重要性"排序）

### 2.1 S 级 — 必做（理论核心 + 汇报效果最佳）

#### Phase B — 死锁专题（OS 课程最核心的同步问题）

**B1. 死锁 4 条件 + 复现实验（哲学家就餐）**

- **理论**：互斥 / 占有并等待 / 不可剥夺 / 循环等待
- **实现要点**：
  - 用 5 个 `sem_open(1)` 模拟 5 把叉子
  - 5 个子进程做"哲学家"：先 `sem_wait(left_fork)`，再 `sem_wait(right_fork)`
  - 故意让 5 个哲学家同时拿左叉 → `ps` 中 5 个进程长期 SLEEPING
- **测试程序**：`user/dining.c`
- **汇报亮点**：直接观察 5 个 SLEEPING 进程；控制台输出 `[DEADLOCK] 5 processes sleeping`
- **日志文件**：`docx/tfc/log/dining.txt`
- **估时**：1d

**B2. 死锁预防（破坏 4 条件之一）**

- **方案 A — 破坏"占有并等待"**：`user/dining_safe1.c`，加一个 `room` 信号量（init=4），最多 4 个哲学家进入
- **方案 B — 破坏"循环等待"**：`user/dining_safe2.c`，奇偶号哲学家取叉顺序不同
- **测试**：连续运行 1000 次，统计死锁次数 → 0
- **汇报亮点**：与 B1 形成强对比，破坏哪个条件、为什么
- **日志文件**：`docx/tfc/log/dining_safe1.txt`、`docx/tfc/log/dining_safe2.txt`
- **估时**：1d

**B3. 死锁避免 — 银行家算法（OS 课最具代表性的算法）**

- **理论**：安全状态 / 安全序列 / 工作向量 / Need 矩阵
- **数据结构**（`kernel/banker.h`）：
  ```c
  #define NRES 8
  struct banker_state {
    int available[NRES];
    int max[NPROC][NRES];
    int allocation[NPROC][NRES];
    int need[NPROC][NRES];  // = max - allocation
  };
  ```
- **系统调用**：
  - `sys_banker_init(nres, avail[])` — 初始化资源向量
  - `sys_banker_request(pid, req[])` — 申请资源（含安全性检查）
  - `sys_banker_release(pid, rel[])` — 释放资源
- **核心算法**：`is_safe_state()` 实现安全性算法
- **测试**：
  - `user/bankertest.c`：经典 5 进程 3 资源示例，验证找到安全序列
  - `user/banker_unsafe.c`：构造一个让算法拒绝的不安全请求
- **汇报亮点**：教材必讲；可演示"银行家拒绝了一个请求，因为会进入不安全状态"
- **日志文件**：`docx/tfc/log/bankertest.txt`、`docx/tfc/log/banker_unsafe.txt`
- **估时**：3-4d

**B4. 死锁检测 + 恢复（最小实现）**

- **实现**：
  - 内核周期性（每 N tick）扫描 `wait_for_lock` 关系，建等待图
  - 用 DFS 检测环
  - 发现死锁后**剥夺（abort）最年轻进程**并释放其资源
- **测试**：让哲学家就餐死锁自动恢复，输出 `[DEADLOCK] abort pid=X`
- **汇报亮点**：自动检测+恢复，无需人为介入
- **日志文件**：`docx/tfc/log/deadlock_detect.txt`
- **估时**：2-3d

#### Phase C — 管程 + 条件变量（与信号量并列的高级同步原语）

**C1. 管程（Monitor）**

- **理论**：Hoare 管程 vs Mesa 管程；条件变量 `wait` / `signal` / `broadcast`
- **数据结构**（`kernel/monitor.h`）：
  ```c
  #define NMON 8
  #define NCOND 4
  struct monitor {
    char name[16];
    struct spinlock lock;        // 管程互斥锁
    int owner;                   // 当前持有管程的进程 pid
    struct cv {                  // 条件变量
      int signaled;              // Mesa 风格
      struct spinlock lock;
      struct proc *waiters;
    } cond[NCOND];
  };
  ```
- **系统调用**：
  - `sys_mon_create(name)` / `sys_mon_enter(mid)` / `sys_mon_exit(mid)`
  - `sys_cv_wait(mid, cvid)` / `sys_cv_signal(mid, cvid)` / `sys_cv_broadcast(mid, cvid)`

**C2. 用管程重写生产者-消费者**

- 把 `semtest3` 改为 `monitortest.c`：3 个生产者 / 3 个消费者
- 对比信号量版本和管程版本的代码复杂度
- **汇报亮点**：与 semtest3 对比代码复杂度，展示"管程把锁的细节封装"
- **日志文件**：`docx/tfc/log/monitortest.txt`
- **估时**：3-4d

#### Phase A2 + D1 — 优先级调度 + 优先级继承（Mars Pathfinder 经典故事）

**A2. 优先级调度（含 aging 解决饥饿）**

- **理论**：低优先级饥饿 + 优先级反转
- **实现**：
  - 复用 `struct proc.priority`（已存在）
  - **Aging**：等待时间越长优先级越高
  - **优先级继承**：高优先级等待低优先级持锁时，临时提升低优先级
- **测试**：`user/prioritytest.c`（验证 aging）+ `user/pi_test.c`（验证继承）

**D1. Mars Pathfinder Bug 复现**

- 高、中、低三优先级进程，低优先级持锁，中优先级不持锁但消耗 CPU
- 观察：高优先级因等待低优先级无法运行
- 加入优先级继承后：高优先级直接继承低优先级优先级，抢占中优先级
- **测试程序**：`user/pathfinder.c`
- **汇报亮点**：1997 年 NASA 火星探测器真实 bug，工业界经典案例
- **日志文件**：`docx/tfc/log/prioritytest.txt`、`docx/tfc/log/pathfinder.txt`
- **估时**：3-4d

### 2.2 A 级 — 强烈推荐

#### Phase F — 实时调度（RM / EDF）

**F1. Rate-Monotonic（RM）**

- **理论**：周期越短优先级越高；可调度条件 ∑(Cᵢ/Tᵢ) ≤ n(2^(1/n) - 1)
- **系统调用**：`sys_rt_register(period, cost)` 注册实时任务
- 内核为实时任务维护独立的优先级表，调度时优先选实时任务

**F2. Earliest Deadline First（EDF）**

- 动态优先级 = 截止时间最近；用截止时间最小堆

**F3. 测试**：`user/rmtest.c` & `user/edftest.c`，构造 3 个周期性任务，验证最坏情况下截止时间达成率

- **汇报亮点**：清晰展示"普通调度追求吞吐 vs 实时调度追求截止时间"
- **日志文件**：`docx/tfc/log/rmtest.txt`、`docx/tfc/log/edftest.txt`
- **估时**：3-5d

#### Phase E — 多核调度（NCPU=8 真正利用）

**E1. Per-CPU 调度队列**

- 现状：所有 CPU 共用 `proc[]` + 单一 `scheduler()`（单核串行扫描）
- 改造：每 CPU 维护 `struct proc *runq[NRUN]`，降低锁粒度

**E2. 负载均衡（Pull 策略）**

- 空闲 CPU 从繁忙 CPU 拉一个进程，每 N tick 触发一次

**E3. 多核同步原语压测**

- `user/spinbench.c`：8 线程争抢一把自旋锁
- `user/mp_bal.c`：8 worker + 8 sleep，观察各 CPU runq 长度
- **汇报亮点**：从单核到多核的可观测变化
- **日志文件**：`docx/tfc/log/mp_bal.txt`、`docx/tfc/log/spinbench.txt`
- **估时**：5-7d

### 2.3 B 级 — 可选

#### Phase D2 — 消息队列

- `sys_mq_send / mq_recv`，内核维护简单固定大小环形队列
- `user/mqtest.c` 测试
- **日志文件**：`docx/tfc/log/mqtest.txt`
- **估时**：2d

---

## 3. 测试计划

### 3.1 通用测试流程

```bash
cd /home/tfc/OS/OS_xv6_riscv
make clean && make
make qemu
```

### 3.2 各 Phase 测试矩阵

| Phase | 测试程序 | 验证目标 | 日志文件 |
|-------|----------|----------|----------|
| B1 | `dining` | 5 个进程长期 SLEEPING | `log/dining.txt` |
| B2 | `dining_safe1` / `dining_safe2` | 1000 次无死锁 | `log/dining_safe1.txt` / `log/dining_safe2.txt` |
| B3 | `bankertest` / `banker_unsafe` | 安全序列 / 拒绝不安全 | `log/bankertest.txt` / `log/banker_unsafe.txt` |
| B4 | `dining`（带 detection） | 自动 abort | `log/deadlock_detect.txt` |
| C1-C2 | `monitortest` | 3P-3C 同步 | `log/monitortest.txt` |
| A2 | `prioritytest` / `pi_test` | aging / 继承 | `log/prioritytest.txt` / `log/pi_test.txt` |
| D1 | `pathfinder` | 高优先级不被卡 | `log/pathfinder.txt` |
| F1-F3 | `rmtest` / `edftest` | 截止时间达成率 | `log/rmtest.txt` / `log/edftest.txt` |
| E1-E3 | `spinbench` / `mp_bal` | 多核可观测 | `log/spinbench.txt` / `log/mp_bal.txt` |
| D2 | `mqtest` | 消息队列 IPC | `log/mqtest.txt` |

### 3.3 对比实验设计

| 对比维度 | A 方案 | B 方案 | 汇报效果 |
|----------|--------|--------|----------|
| 死锁复现 vs 预防 | `dining`（5 全拿左叉） | `dining_safe1`（4 限制） | 直接对比 ps 输出 |
| 银行家安全 vs 不安全 | `bankertest`（找到序列） | `banker_unsafe`（被拒绝） | 控制台输出对比 |
| 信号量 vs 管程 | `semtest3` | `monitortest` | 代码复杂度对比 |
| 优先级反转 vs 继承 | `pathfinder`（无继承） | `pathfinder`（开继承） | 高优先级响应时间 |
| 单核 vs 多核 | 单核 throughput | 多核 throughput | 加速比 |

### 3.4 性能基准（复用现有 cgettimeofday / schedstat）

- `schedlatency`：调度延迟基线（B3 / E2 用于对比）
- `throughput`：吞吐量基线（E1 Per-CPU 队列后加速比）
- `schedstat`：B4 / E2 / F3 扩展统计字段

---

## 4. 排期建议

| 顺序 | Phase | 内容 | 估时 | 汇报价值 | 累计 |
|------|-------|------|------|----------|------|
| 1 | B1 | 哲学家就餐复现 | 1d | ⭐⭐⭐⭐⭐ | 1d |
| 2 | B2 | 死锁预防 2 种方案 | 1d | ⭐⭐⭐⭐ | 2d |
| 3 | B3 | 银行家算法 | 3-4d | ⭐⭐⭐⭐⭐ | 6d |
| 4 | B4 | 死锁检测 + 恢复 | 2-3d | ⭐⭐⭐⭐ | 9d |
| 5 | C1 | 管程 + 条件变量 | 2d | ⭐⭐⭐⭐ | 11d |
| 6 | C2 | 管程重写 P-C | 1-2d | ⭐⭐⭐ | 13d |
| 7 | A2 | 优先级 + aging + 继承 | 2d | ⭐⭐⭐⭐ | 15d |
| 8 | D1 | Mars Pathfinder 复现 | 1-2d | ⭐⭐⭐⭐⭐ | 17d |
| 9 | F1 | Rate-Monotonic | 1-2d | ⭐⭐⭐ | 19d |
| 10 | F2 | EDF | 1-2d | ⭐⭐⭐ | 21d |
| 11 | F3 | 实时任务测试 | 1d | ⭐⭐⭐ | 22d |
| 12 | E1 | Per-CPU 队列 | 3d | ⭐⭐⭐ | 25d |
| 13 | E2 | 负载均衡 | 2-3d | ⭐⭐ | 28d |
| 14 | E3 | 多核压测 | 1d | ⭐⭐ | 29d |
| 15 | D2 | 消息队列（可选） | 2d | ⭐⭐ | 31d |

---

## 5. 与现有规划的衔接与更新

`docx/tfc/ProcessMgmt_Scheduling_NextPhase.md`（早一版规划）已规划 A1-A3 / B1-B4 / C1-C2 / D1-D2 / E1-E3 / F1-F3。

**主要差异与更新**：
- **A1（SJF）已实际实现并验证**（`user/sjfbusy.c`、`Done.md` 1775-1788），从下一阶段规划中移除
- 在 B / C / A2+D1 上推荐 **优先实施**（汇报价值最高的 3 个 Phase）
- F（实时）和 E（多核）作为 A 级推荐，与"完整理论版"目标对齐
- D2（消息队列）降为 B 级可选

---

## 6. 汇报演示脚本（建议）

为最大化汇报效果，建议按以下顺序展示：

| 段 | 时长 | 内容 | 对应 Phase |
|----|------|------|------------|
| 1. 进程基础 | 5min | fork/exec/wait/waitpid 演示 | L1 已完成 |
| 2. 同步互斥 | 10min | 信号量 P-C、管程 P-C 对比 | L2 已完成 + C2 |
| 3. 死锁专题（重点） | 15min | 复现 → 预防 → 银行家 → 自动检测 | B1 → B2 → B3 → B4 |
| 4. 调度算法 | 10min | RR/FCFS/SJF/MLFQ + 优先级反转 | L4 已完成 + A2 / D1 |
| 5. 高级调度 | 10min | RM/EDF（可调度条件）+ 多核 Per-CPU | F1-F3 + E1-E3 |
| Q&A | 10min | — | — |

---

## 7. 验收标准（覆盖 OS 课程进程管理 + 调度教学大纲）

完成后应能在教学报告中回答：

1. 什么是进程？PCB 包含什么？xv6 如何管理？
2. 进程状态有哪些？互相如何转换？
3. 进程间通信方式有哪些？（shm / msgq / pipe / signal）
4. 信号量和管程的本质区别？什么场景用哪个？
5. 什么是死锁？产生的 4 个条件？如何处理（预防/避免/检测/恢复）？
6. 银行家算法的安全性检查如何进行？
7. 经典的进程调度算法有哪些？各自的优缺点？
8. MLFQ 为什么能同时优化响应时间和吞吐？
9. 什么是优先级反转？xv6 如何用优先级继承避免？
10. 多核环境下调度器需要解决什么新问题？
11. 实时调度和普通调度的本质区别？

---

## 8. 文档与代码产物的层次组织

每个 Phase 完成后，按以下四个层次产出：

```
1. 设计文档 (docx/tfc/<topic>_Design.md)        — 理论背景、算法描述、数据结构、API 草案
2. 实现记录 (docx/tfc/<topic>_Impl.md)          — 关键代码片段、改动文件列表、踩过的坑
3. 测试日志 (docx/tfc/log/<topic>.txt)          — 实际运行日志 + 预期对比
4. 完成条目 (更新到 docx/tfc/Done.md)            — 一段简洁的"已验证 X / Y / Z"
```

---

## 9. 关键文件改动预期清单

| Phase | 主要新增 / 修改文件 |
|-------|---------------------|
| B1-B2 | `user/dining.c`、`user/dining_safe1.c`、`user/dining_safe2.c` |
| B3 | `kernel/banker.h`、`kernel/banker.c`、`kernel/sysproc.c`（加 banker_init/request/release）、`kernel/syscall.h`（#40-42）、`user/bankertest.c`、`user/banker_unsafe.c` |
| B4 | `kernel/deadlock_detect.c`（等待图 + DFS）、扩展 `sys_schedstat` |
| C1-C2 | `kernel/monitor.h`、`kernel/monitor.c`、`kernel/sysproc.c`（#43-48）、`user/monitortest.c` |
| A2+D1 | `kernel/proc.c` 优先级 + aging + 继承逻辑、`user/prioritytest.c`、`user/pi_test.c`、`user/pathfinder.c` |
| F1-F3 | `kernel/rt.c`（RM/EDF 最小堆）、`kernel/sysproc.c` 加 rt_register、`user/rmtest.c`、`user/edftest.c` |
| E1-E3 | `kernel/proc.c` 改 Per-CPU runq、`kernel/start.c` 启动多核、`user/spinbench.c`、`user/mp_bal.c` |
| D2 | `kernel/msgq.c`、`user/mqtest.c` |

---

## 10. 不在本阶段范围内（明确排除）

- 文件系统 / 设备驱动 / 虚拟内存 → 属其他章节
- 网络栈 → 属其他章节
- 用户态线程库（pthread）→ 与内核进程模型正交，可独立做
- 调度器的形式化证明（Petri 网 / 排队论）→ 教学报告不需要
- CFS（Linux 红黑树调度）→ 与 MLFQ 思想相近，不重复实现

---

## 11. 文档版本

- **初版**：2026-06-16
- **状态**：待评审
- **下一步**：在 S 级 Phase B1（哲学家就餐）启动前，先在 `Todo.md` 中补齐本期所有待办条目
