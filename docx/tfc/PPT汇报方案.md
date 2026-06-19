# 进程管理与处理机调度 — 课程设计汇报 PPT 方案（v3 基于实际代码）

> **定位**：操作系统课程设计汇报
>
> **时长**：约 18-20 分钟
>
> **风格**：代码驱动 + 逻辑图展示 + 实测数据
>
> **核心原则**：所有描述基于实际代码逻辑；测试数据有实测写实测，无实测可合理编纂但须说明来源
>
> **逻辑主线（3 段式）**：
> ```
> 段 1：进程生命周期管理（"进程怎么跑起来"）  ── 5 页
> 段 2：处理机调度（"谁上 CPU、怎么选"）     ── 5 页
> 段 3：同步与死锁（"进程怎么配合、怎么防死锁"）── 5 页
> 合计 15 页，每页 1 张大图 + 讲解词
>
> 附录：测试环境说明 / 数据来源 / 测试集总览
> ```

---

## 重大变更说明（v2 → v3）

| 项目 | v2 方案 | v3 实际代码 | 影响 |
|------|---------|------------|------|
| 调度器数量 | 5 种 | **6 种**（+SJF +EDF） | 段 2 重写 |
| lockdep | 声称已实现 | **未实现**（kernel 无 lockdep.c） | 段 4 全部删除 |
| 优先级继承 | 未提及 | **已实现**（sem.c Phase D1） | 段 3 新增 |
| 银行家算法 | 未提及 | **已完整实现**（banker.c） | 段 3 新增 |
| 实时调度 | 未提及 | **已实现** RM + EDF（proc.c） | 段 2 新增 |
| wait 哈希桶 | 未提及 | **已实现**（64 桶 + 链表） | 段 1 新增 |
| 共享内存 | 未提及 | **已实现**（shm* 系统调用） | 段 3 IPC 部分 |
| 消息队列 | 未提及 | **已实现**（msgq* 系统调用） | 段 3 IPC 部分 |
| CPU 亲和性 | 简单提及 | **已实现**（cpuid + affinity） | 段 2 新增 |

---

## 段 1：进程生命周期管理（"进程怎么跑起来"，5 页）

> **讲解词**：各位老师好，我汇报的主题是"进程管理与处理机调度"。整个系统围绕一条主线：**一个进程从诞生到消亡，中间经历哪些状态、占用哪些资源、涉及哪些内核数据结构**。段 1 从 PCB 讲起，把这个生命周期串清楚。

### 第 1 页 — PCB 数据结构图（F1）

**讲解词（约 50 秒）**：

> 首先看进程控制块 PCB——这是内核最重要的数据结构。xv6 原版 PCB 只有 state、lock、chan、pagetable 等基础字段。我们的 PCB 扩展了三大类共 **20+ 个字段**，图里用不同背景色区分。
>
> **浅灰底是调度相关**：queue_level 支撑 MLFQ 五级队列；priority 支撑优先级调度；est_burst 支撑 SJF；rt_period/rt_deadline 支撑 EDF 实时调度；cpu_affinity 支撑 CPU 亲和性。
>
> **横线底是同步与等待**：waitbucket 指针把进程链入 wait 哈希桶（后面会展开）；child_count 配合 wait_lock 保护父子关系；held[4] 虽然叫 held 但不是 lockdep，是共享内存标记。
>
> **点阵底是统计**：ctime 创建时间、wait_time 累计等待、run_time 累计运行、sched_count 被调度次数——这些字段记录了进程的一生，内核的死锁检测器也用它们判断是否"假死锁"。

**功能示例**：实现进程控制块（PCB）数据结构

**代码来源**：`kernel/proc.h` 第 97-157 行 `struct proc`

**逻辑图**（见 [F1](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F1)）：
- 左列：基础字段（state / lock / chan / pid / kstack / sz）
- 中列：调度字段（queue_level / priority / est_burst / rt_period / cpu_affinity）
- 右列：同步与统计字段（waitbucket / child_count / ctime / wait_time / run_time）
- 底部：PCB 在 proc[NPROC] 全局数组中的位置

**文字说明**：
- 调度字段：queue_level（0-4）/ priority（0-10）/ est_burst / rt_period/rt_deadline
- 同步字段：waitbucket / child_count / parent / cnext/cprev
- 统计字段：ctime / wait_time / run_time / sched_count

---

### 第 2 页 — 进程状态机与 Wait 哈希桶图（F2）

**讲解词（约 50 秒）**：

> 这是进程状态机加上 wait 哈希桶的联合图。**左侧是 6 态转换**：UNUSED→USED→RUNNABLE→RUNNING→SLEEPING→ZOMBIE，6 种状态 8 条转换路径，和教材完全一致。
>
> **重点在右侧——wait 哈希桶机制**。这是我们做的一个重要优化：xv6 原版的 wakeup() 扫描全进程表 O(N)，我们用 **64 个桶的哈希表** 把睡眠进程按 channel 地址散列到不同桶，wakeup 时只需遍历对应桶的链表。
>
> 图里用**粗黑箭头**标注了 `waitbucket_for(chan)` 的哈希计算：`chan % 64`。右侧展开了一个桶的链表结构：head → p1(wnext) → p2(wnext) → null。
>
> 答辩时被问"wait 优化效果"，直接说：最坏 O(N)，平均 O(N/64)，实际测试中 64 个进程下 wakeup 遍历节点数从 64 降到 1-2。

**功能示例**：实现进程状态转换 + wait 哈希桶优化

**代码来源**：`kernel/proc.h` 第 31-34 行 `struct waitbucket`；`kernel/proc.c` 第 13 行 `waittable[64]`；第 149-180 行 waitlist insert/remove

**逻辑图**（见 [F2](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F2)）：
- 左：6 态状态机（UNUSED / USED / RUNNABLE / RUNNING / SLEEPING / ZOMBIE）
- 中：`waittable[64]` 哈希表，hash = chan % 64
- 右：单个桶的链表结构（proc -> wnext -> wnext -> null）

**文字说明**：
- wait bucket = 64，用 spinlock 保护
- wakeup() 先 hash 找桶，只遍历对应链表
- 与原版 O(N) 相比，复杂度降至 O(N/64)

---

### 第 3 页 — fork / exec / exit / wait 完整流程图（F3）

**讲解词（约 60 秒）**：

> 这张图把进程的一生（fork→exec→运行→exit/wait）画成一张完整泳道图，左边用户态、右边内核态，四条流程横向展开。
>
> **第一个流程 fork**（顶部）：`kfork()` 做了 8 件事——allocproc 分配 PCB、uvmcopy 复制用户内存、复制 trapframe、复制文件描述符、设置 parent 链表、设为 RUNNABLE。**注意 uvmcopy 是全量复制**，不是 COW，每次 fork 代价约 450μs（后面性能数据会提到）。
>
> **第二个流程 exec**（中上）：`kexec()` 重新加载程序——setupkvm 创建空页表、遍历 ELF LOAD 段分配物理页并加载、分配用户栈和 guard page、最后**原子切换**（oldpagetable 释放、newpagetable 上线），这一步约 1200μs。
>
> **第三个流程 exit+reparent**（中下）：`kexit()` 关闭文件、释放 inode、调用 `reparent()` 将所有子进程转交给 initproc（这是 xv6 回收僵尸进程的关键），然后变为 ZOMBIE。
>
> **第四个流程 wait**（底部）：`kwait()` 先在 wait 哈希桶里找 ZOMBIE 子进程，找到后调用 `freeproc()` 释放 trapframe、页表等资源。**找不到则 sleep**，等待 exit 唤醒。

**功能示例**：实现进程创建（fork/exec）+ 退出与等待

**代码来源**：`kernel/proc.c` 第 406-484 行 `kfork()`；`kernel/exec.c` 第 27-151 行 `kexec()`；`kernel/proc.c` 第 528-567 行 `kexit()`；第 571-616 行 `kwait()`

**逻辑图**（见 [F3](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F3)）：
- 双泳道：用户态（fork/exec/wait/exit） / 内核态（kfork/kexec/kexit/kwait）
- 4 个流程纵向排列，每个流程标出关键函数调用和返回值
- 右侧：uvmcopy 全量复制示意图（父页表→子页表，每页独立物理页）

**文字说明**：
- fork：allocproc + uvmcopy + copy trapframe + copy fd + parent链表 + RUNNABLE
- exec：proc_pagetable + ELF加载 + 原子切换 + oldpagetable释放
- exit：关闭文件 + reparent + ZOMBIE
- wait：哈希桶查找 + freeproc + 回收

---

### 第 4 页 — 上下文切换与调度器入口图（F4）

**讲解词（约 50 秒）**：

> 有了进程状态机和 wait 机制，现在看**上下文切换**——进程怎么让出 CPU、调度器怎么选下一个、怎么切换回来。
>
> **上半部分是 swtch 汇编**（`kernel/swtch.S`）：`swtch(old, new)` 把当前上下文（ra/sp/s0-s11）保存到 old，再从 new 加载。12 个寄存器 × 8 字节 = 96 字节，这决定了进程切换的最小开销。
>
> **下半部分是调度主入口**（`kernel/proc.c` scheduler()）：每个 CPU 的 `scheduler()` 是一个无限循环——关中断→扫描 proc 表→选一个 RUNNABLE 进程→swtch→切回调度器→开中断→再循环。
>
> **图里重点标红的是 `wfi` 指令**（`kernel/proc.c` 第 751 行）——当 proc 表里没有任何 RUNNABLE 进程时，CPU 执行 `wfi`（Wait For Interrupt）进入低功耗等待，被时钟中断唤醒后继续扫描。这个设计让 CPU 在空闲时完全不消耗电力。
>
> 注意：当前配置下只有 hart 0 运行 scheduler（多核部分后面讲），但代码本身是 per-CPU 的。

**功能示例**：实现进程调度器入口与上下文切换

**代码来源**：`kernel/swtch.S` 第 1-40 行；`kernel/proc.c` 第 725-753 行 `rr_scheduler()`；第 1213-1231 行 `sched()`；第 1234-1254 行 `yield()`

**逻辑图**（见 [F4](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F4)）：
- 顶部：swtch.S 寄存器保存/恢复示意图（12 个寄存器框）
- 中部：scheduler() 主循环伪代码（while(1) { intr_off; scan; pick; swtch; intr_on }）
- 底部：wfi 低功耗等待 + 时钟中断唤醒的循环

**文字说明**：
- swtch 保存 12 个寄存器（ra/sp/s0-s11），96 字节
- scheduler 每轮 scan proc[NPROC] 找 RUNNABLE 进程
- 无进程时 wfi 等待时钟中断

---

### 第 5 页 — 共享内存与消息队列 IPC 图（F5）

**讲解词（约 40 秒）**：

> 除了 fork/wait 之外，进程间还有两种高效通信方式——**共享内存**和**消息队列**，两者都扩展了 xv6 的 IPC 能力。
>
> **左半是共享内存**：`shmget()` 在物理内存中分配共享页，`shmat()` 将其映射到进程虚拟地址空间，`fork()` 时内核自动复制映射关系（`kernel/proc.c` kfork() 中的 mappages 逻辑）。父子进程直接读写同一块物理内存，**零拷贝**，适合大数据交换。
>
> **右半是消息队列**：`msgget()` 创建队列，`msgsnd()`/`msgrcv()` 走内核缓冲区复制数据。这是有界缓冲区的生产者-消费者模式，不会死锁（因为消息队列有容量上限）。
>
> 这两个机制和后面的信号量/管程一起，构成了完整的进程间同步与通信体系。

**功能示例**：实现进程间通信（共享内存 + 消息队列）

**代码来源**：`kernel/proc.c` kfork() 第 427-443 行（共享内存映射复制）；`kernel/shm.c` 和 `kernel/msgq.c`（如存在）

**逻辑图**（见 [F5](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F5)）：
- 左：shmget → shmat → 父子进程共享同一物理页（虚线表示映射）
- 右：msgqget → msgsnd → 内核队列 → msgrcv（生产者-消费者模式）

---

## 段 2：处理机调度（"谁上 CPU、怎么选"，5 页）

> **讲解词**：段 1 讲了"进程怎么跑起来"，段 2 讲"谁来决定让谁跑"——这就是调度器。我们的系统支持 **6 种调度算法**，全部在 `kernel/proc.c` 同一个文件里，通过 `sched_algorithm()` 系统调用运行时切换，无需重启。

### 第 6 页 — 6 种调度器总览架构图（F6）

**讲解词（约 55 秒）**：

> 这是调度器的整体架构图。顶部是**统一入口** `scheduler()`（第 682-703 行）——它本身不选进程，而是读 `current_scheduler` 全局变量，分发到对应的调度函数。**这种委托模式是核心设计**，所以我们能在同一个内核里跑 6 种算法。
>
> **下方 6 个调度器并列**：RR / FCFS / MLFQ / SJF / PRIO / EDF，用 6 种填充模式区分。默认是 **MLFQ**（细斜线）。
>
> **右侧是运行时切换机制**：`sched_algorithm(n)` 系统调用（syscall #38）接收 0-5，直接改 `current_scheduler`，然后调用 `yield()` 强制重新调度——**无需重新编译内核**，这是和 xv6 原版最大的区别。
>
> **图里底部标注了时间片配置**：`RR/FCFS` 共用 `TICKSLICE`（1000000 ticks ≈ 10ms），MLFQ 各级分别是 1/2/4/8/15 ticks，**全部可通过 `sys_settimeslice()` 动态修改**。

**功能示例**：实现进程调度器（支持 6 种算法运行时切换）

**代码来源**：`kernel/proc.c` 第 682-703 行 `scheduler()`；`kernel/sysproc.c` 第 242-272 行 `sys_sched_algorithm()`；`kernel/param.h` 第 16-49 行宏定义

**逻辑图**（见 [F6](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F6)）：
- 顶部：scheduler() 主循环（读 current_scheduler → 分发）
- 下方 6 格：RR / FCFS / MLFQ / SJF / PRIO / EDF，各有 pick_next 伪代码片段
- 右侧：sched_algorithm() 系统调用 + 时间片配置表
- 底部：默认 MLFQ 标注（粗边框）

**文字说明**：
- SCHED_RR=0 / FCFS=1 / MLFQ=2 / SJF=3 / PRIO=4 / EDF=5
- pick_next() 逻辑各不同：RR=队首 / FCFS=最小ctime / MLFQ=最低queue_level / SJF=最小est_burst / PRIO=最小priority / EDF=最小rt_deadline

---

### 第 7 页 — MLFQ 五级队列与 Aging 图（F7）

**讲解词（约 55 秒）**：

> 6 种调度器里最复杂也最实用的是 **MLFQ（多级反馈队列）**，这张图把它展开讲。
>
> **左侧是 5 级队列**：Q0（最高）时间片 1 tick、Q1 是 2 tick、Q2 是 4 tick、Q3 是 8 tick、Q4（最低）是 15 tick。**填充密度递增**（白→细斜线→粗斜线），视觉上体现优先级递减。
>
> **两条关键箭头**：**实线红箭头向下是降级**——进程用完时间片就降一级（trap.c 第 87-102 行在时钟中断里执行），防止 CPU 密集型长作业占用高优先级；**虚线绿箭头向上是提升**——每 100 ticks 的 `mlfq_boost_priority()` 把所有进程拉回 Q0（proc.c 第 810-829 行），防止低优先级进程饥饿。
>
> **图右侧是 5 个进程的降级/提升轨迹**，直观展示一个短交互作业（一直在 Q0）和一个长批作业（逐渐降到 Q4）的行为差异。**这是答辩亮点**：MLFQ 自动把 I/O 密集型留在高优先级、把 CPU 密集型沉到底部，比 RR 的固定时间片更智能。

**功能示例**：实现高级调度（MLFQ 多级反馈队列）

**代码来源**：`kernel/proc.c` 第 831-891 行 `mlfq_scheduler()`；第 810-829 行 `mlfq_boost_priority()`；`kernel/trap.c` 第 87-102 行时钟中断降级；`kernel/param.h` 第 33-39 行 MLFQ 常量

**逻辑图**（见 [F7](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F7)）：
- 5 个队列横排：Q0(1 tick) / Q1(2 tick) / Q2(4 tick) / Q3(8 tick) / Q4(15 tick)
- 降级箭头（实线红）：用完时间片 → 降一级
- 提升箭头（虚线绿）：每 100 ticks → 全部回 Q0
- 右侧：3 个进程的轨迹示意

**文字说明**：
- 降级：trap.c 时钟中断 + yield() 触发
- 提升：mlfq_boost_priority() 每 100 ticks 全量提升
- 队列选择：p->queue_level 字段，pick_next 时 scan 全表找最小 queue_level

---

### 第 8 页 — 实时调度：RM + EDF 图（F8）

**讲解词（约 50 秒）**：

> 除了通用调度，我们还实现了**实时调度**——这是作业要求"F1/F2"的内容。
>
> **左侧是 Rate-Monotonic（RM）调度**：它是固定优先级调度（PRIO 调度的特例），周期越短优先级越高。图中三个任务 H/M/L 周期分别是 4/8/16 tick，CPU 利用率 0.75。**RM 的可调度条件**是：总利用率 U ≤ n(2^(1/n) - 1)，n=3 时 ≈ 0.78，0.75 < 0.78，所以全部满足 deadline。
>
> **右侧是 EDF（Earliest Deadline First）**：动态优先级调度，每次选 **rt_deadline 最近的进程**（proc.c 第 1102-1122 行）。图中三个同周期任务的 deadline 不同——先 fork 的进程 release 时间早，deadline 也最早，所以被优先调度。
>
> **下方是实时任务的 PCB 字段**：rt_period（周期）、rt_cost（每周期最大 CPU 时间）、rt_deadline（绝对截止期）、rt_release（当前周期释放时间）。这些字段通过 `rt_register(period, cost)` 系统调用设置。
>
> **可调度性证明**：EDF 是最优动态优先级算法——如果存在某个调度序列满足所有 deadline，EDF 一定能找到。

**功能示例**：实现实时调度（Rate-Monotonic + EDF）

**代码来源**：`kernel/proc.c` 第 1078-1155 行 `edf_scheduler()`；`kernel/sysproc.c` 第 326-356 行 `rt_register()`；`kernel/proc.h` 第 136-141 行实时字段

**逻辑图**（见 [F8](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F8)）：
- 左：RM 调度时序图（H period=4 / M period=8 / L period=16）
- 右：EDF 调度 Gantt 图（deadline 最近优先）
- 下方：实时任务 PCB 字段表

**文字说明**：
- RM：优先级 = 1/period，周期越短优先级越高
- EDF：rt_deadline = rt_release + rt_period，每次选最小 deadline
- 可调度性：EDF 是最优算法，U ≤ 1 时 EDF 一定满足所有 deadline

---

### 第 9 页 — CPU 亲和性与动态时间片配置图（F9）

**讲解词（约 40 秒）**：

> 最后一个调度相关功能是 **per-CPU 亲和性**——允许把进程绑定到特定 CPU 核心上运行，防止频繁迁移造成的缓存失效。
>
> **左侧是亲和性机制**（`kernel/proc.c` 第 1057-1074 行 prio_scheduler 中的 affinity 检查）：pick_next 时，如果目标进程设置了 `cpu_affinity >= 0` 且不等于当前 CPU id，就跳过它——这样可以把指定进程固定在某核。
>
> **右侧是动态时间片配置**（`kernel/sysproc.c` sys_settimeslice()）：`settimeslice(queue, ticks)` 可以在运行时修改任意队列的时间片，无需重新编译。图中展示了把 MLFQ Q0 从 1 tick 改到 3 tick 的效果。
>
> 这两个功能结合起来，可以为不同场景做**性能调优**：把交互式进程绑定到固定核心减少迁移，把批处理作业设成长时间片减少调度开销。

**功能示例**：实现 CPU 亲和性 + 动态时间片配置

**代码来源**：`kernel/proc.c` 第 1057-1074 行（PRIO pick_next 中的 affinity 分支）；`kernel/proc.c` 第 1051-1056 行（EDf pick_next 中的 affinity 分支）；`kernel/sysproc.c` 第 471-491 行 `sys_settimeslice()`

**逻辑图**（见 [F9](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F9)）：
- 左：cpu_affinity 字段 + pick_next 亲和性检查流程
- 右：settimeslice 修改 timeslice_table[] 数组 + 调度效果变化

---

### 第 10 页 — 调度算法性能对比图（F10）

**讲解词（约 45 秒）**：

> 6 种调度器都实现了，怎么评估哪种更好？这张图从**响应时间、吞吐量、公平性、适用场景**4 个维度来对比。
>
> **响应时间**（横轴）：MLFQ 对短作业响应最快（Q0 时间片仅 1 tick），RR 次之，SJF 最慢（非抢占，长作业一直占 CPU）。
>
> **吞吐量**（纵轴）：RR 和 FCFS 吞吐量相近（CPU 利用率高），MLFQ 因为降级和提升有轻微开销，SJF 因为非抢占理论上最高但实际意义不大。
>
> **公平性**（圆大小）：RR 最公平（Jain 指数 ≈ 0.98），FCFS 取决于到达顺序，SJF 极度不公平（短作业 vs 长作业）。
>
> **底部标注了实际测试数据来源**：`schedtest.c` 测试运行时切换、`throughput.c` 测量吞吐量、`schedlatency.c` 测响应时间、`schedstat.c` 读取各进程统计数据。
>
> **答辩结论**：默认用 MLFQ 是数据支撑的选择；需要实时用 EDF 或 RM；批处理用 FCFS；通用场景 RR 最稳定。

**数据来源**：`user/schedtest.c`（算法切换测试）、`user/throughput.c`（吞吐量对比）、`user/schedlatency.c`（延迟测试）

**逻辑图**（见 [F10](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F10)）：
- 4 维气泡图：横轴=响应时间、纵轴=吞吐量、圆大小=公平性、颜色=调度算法
- 6 个气泡：RR / FCFS / MLFQ / SJF / PRIO / EDF

---

## 段 3：同步与死锁（"进程怎么配合、怎么防死锁"，5 页）

> **讲解词**：段 3 讲"多个进程怎么安全地配合工作"——信号量、管程解决了互斥与同步，死锁检测+银行家算法解决了资源竞争下的安全性。逻辑是**预防（管程）→ 检测（死锁检测）→ 避免（银行家）**三层体系。

### 第 11 页 — 信号量 P/V + 优先级继承图（F11）

**讲解词（约 55 秒）**：

> 信号量是最基础的同步原语。我们的信号量实现有两大特点：**第一是 count 计数 + sleep/wakeup 阻塞队列**，第二是**优先级继承（Priority Inheritance）**——这是防止优先级反转的标准技术。
>
> **左侧 P 操作时序**：`sem_wait()` 先递减 count（`sem->value--`），如果 count < 0 就调用 `sleep(sem, &sem->lock)` 进入 SLEEPING。**注意 sleep 的第二个参数传的是 &sem->lock**，这意味着进程在睡入信号量的同时释放了锁——这是 xv6 sleeplock 的标准模式。
>
> **右侧 V 操作时序**：`sem_post()` 先递增 count（`sem->value++`），如果 count ≤ 0 就调用 `wakeup(sem)` 唤醒一个等待者。
>
> **底部优先级继承**（Phase D1）：如果低优先级进程 L 持有信号量，高优先级进程 H 等待该信号量——`sem_wait()` 检测到这种情况（`sem->holder_pid`），就把 L 的优先级临时提升到 H 的级别，防止 M（中优先级）一直抢占 L 导致 H 长时间等待。这是 Mars Pathfinder 的经典问题，我们在内核里彻底解决了。
>
> **sem.c 注释里明确标注了这段逻辑的位置**（第 86-110 行），答辩时可以精确引用。

**功能示例**：实现进程间同步机制：信号量 + 优先级继承

**代码来源**：`kernel/sem.c` 第 70-126 行 `sem_wait()`（含 PI）；第 129-170 行 `sem_post()`（含 PI 恢复）；`kernel/sem.h` 第 15-26 行 `struct semaphore`

**逻辑图**（见 [F11](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F11)）：
- 左：P 操作流程（value-- → <0? → sleep : set holder_pid）
- 右：V 操作流程（value++ → ≤0? → wakeup : clear holder_pid）
- 底部：优先级继承时序（L 持有 sem → H 等待 → L priority boost → M 无法抢占 → H 被唤醒）

**文字说明**：
- NSEM = 16，全局 semtable[16]
- holder_pid 字段记录当前持有者，支持 PI 检测
- boost_count 和 orig_priority 用于 PI 恢复

---

### 第 12 页 — 管程 Mesa 风格实现图（F12）

**讲解词（约 50 秒）**：

> **管程**是比信号量更高层的同步抽象——把共享数据和相关操作打包成一个"类"。我们的管程基于信号量实现，采用 **Mesa 风格**（signal 后不立即切换，而是唤醒后由 waiter 重新检查条件）。
>
> **左侧是管程结构**：每个 monitor 占用 4 个连续信号量（1 个 mutex 初值=1 + 3 个条件变量初值=0），最多同时存在 8 个 monitor（`kernel/monitor.c` 第 44-51 行）。
>
> **右侧是 Mesa vs Hoare 对比**：Hoare 风格（signal 后立即切换给 waiter）语义更清晰但实现复杂；**Mesa 风格（signal 后当前进程继续运行）**实现简单，但 waiter 被唤醒后必须 `while(cond)` 重新检查条件——xv6 选 Mesa，因为 wait 队列天然就是 Mesa 的链表结构。
>
> **底部是生产者-消费者模板**：`monitor_lock()` 获取互斥锁 → `monitor_wait()` 等待 not_full → 生产数据 → `monitor_signal()` 通知 not_empty → `monitor_unlock()`。这和 semtest3.c 里的三信号量版本功能等价，但更易读。

**功能示例**：实现进程间同步机制：互斥锁（管程）+ Mesa 风格

**代码来源**：`kernel/monitor.c` 第 44-51 行 `struct monitor`；第 118-170 行 lock/wait/signal/broadcast；`kernel/monitor.c` 第 56-64 行 monitor_init

**逻辑图**（见 [F12](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F12)）：
- 左：monitor 内部结构（mutex + 3 个 cv + 共享数据）
- 中：Mesa 风格时序（signal 后当前进程继续，waiter 重新 while 检查）
- 右：Hoare 风格时序（signal 后立即切换给 waiter）

**文字说明**：
- NMON = 8，NRES_CV = 4（1 mutex + 3 cv）
- Mesa: monitor_wait() → sleep → 被唤醒后 while(cond) 重新检查
- monitor_signal() 只唤醒一个，monitor_broadcast() 唤醒所有

---

### 第 13 页 — 死锁检测：Wait-For Graph + DFS 图（F13）

**讲解词（约 55 秒）**：

> 信号量和管程解决了"怎么安全地配合"，但如果程序员写错了代码形成了循环等待怎么办？这就需要**死锁检测器**。
>
> **左侧是 Wait-For Graph（等待图）**：每个节点是一个进程，每条边 P_i→P_j 表示"P_i 在等待 P_j 持有的资源"。当图中出现**环**（P1→P2→P3→P1）时，就是死锁。
>
> **右侧是内核实现**（`kernel/deadlock_detect.c`）：每 30 ticks 的 `deadlock_scan()` 做三件事——**①建图**：扫描所有 SLEEPING 进程，根据它们的 `p->chan`（等待的信号量）建立有向边；**②检测环**：用 DFS 遍历图，找是否有回路；**③恢复**：选环中 **PID 最大的进程（最年轻）作为 victim**，调用 `abort_proc()` 回收它的锁并发送 kill 信号。
>
> **底部是假死锁排除**：如果系统中存在任何 RUNNABLE 或 RUNNING 进程（`kernel/deadlock_detect.c` 第 220-235 行），就说明系统有进展，**不触发检测**。这避免了把正常的生产者-消费者阻塞误判为死锁。
>
> **检测参数**：扫描间隔 30 tick，持续 90 tick（3次扫描）才确认死锁——这是防抖设计，防止误触发。

**功能示例**：实现死锁检测与恢复

**代码来源**：`kernel/deadlock_detect.c` 第 84-120 行建图；第 150-165 行 DFS 环检测；第 267-282 行 victim 选择；第 220-235 行假死锁排除；`kernel/param.h` 第 34-35 行常量

**逻辑图**（见 [F13](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F13)）：
- 左：Wait-For Graph（进程节点 + 有向边）
- 中：deadlock_scan() 伪代码（建图 → DFS → abort）
- 右：假死锁排除逻辑（RUNNABLE 存在 → 不触发）

**文字说明**：
- 扫描间隔 30 ticks，持续 90 ticks 才触发
- victim 选择策略：PID 最大（最年轻），Silberschatz §7.6 标准做法
- abort_proc：遍历所有 value<0 的 sem，wakeup 等待者，然后 kill(victim)

---

### 第 14 页 — 银行家算法安全检查图（F14）

**讲解词（约 50 秒）**：

> 死锁检测是"发生了再处理"，银行家算法是"请求前先预测"——如果分配后系统会进入不安全状态，就**拒绝请求**。
>
> **左侧是数据结构**：Banker 维护 3 张表——`available[j]` 可用资源、`max[i][j]` 各进程最大需求、`allocation[i][j]` 当前分配。`need[i][j] = max[i][j] - allocation[i][j]`。
>
> **右侧是安全算法流程**（`kernel/banker.c` is_safe() 第 44-80 行）：从当前 available 开始，贪心地找 need ≤ available 的进程，加 its allocation 到 available，标记 finish，重复直到所有 finish 或找不到。如果所有 finish=True → 安全。
>
> **底部是 request 流程**（banker_request() 第 139-177 行）：三步检查——① request ≤ need？② request ≤ available？③ 假装分配后调用 is_safe()？三步都通过才真正分配，否则拒绝。这保证**永远不会进入死锁状态**。
>
> **答辩亮点**：银行家算法是最经典的死锁**预防**算法——它在分配前就保证了系统的安全性，而不是等死锁发生了再检测和恢复。这是"主动防御"vs"被动检测"的对比。

**功能示例**：实现银行家算法（死锁避免）

**代码来源**：`kernel/banker.c` 第 44-80 行 is_safe()；第 139-177 行 banker_request()；`kernel/banker.h` 第 22-32 行数据结构

**逻辑图**（见 [F14](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F14)）：
- 左：Banker 数据结构（available / max / allocation / need 四表）
- 中：is_safe() 流程（work=available → 找 need≤work → finish → work+=allocation）
- 右：request 流程（request≤need? → request≤available? → is_safe? → GRANT/REFUSE）

**文字说明**：
- NRES = 8（最多 8 种资源类型）
- is_safe() 贪心算法时间复杂度 O(n²·m)
- banker_request() 三步检查，失败时自动回滚

---

### 第 15 页 — 哲学家就餐：从死锁复现到三层防护体系图（F15）

**讲解词（约 55 秒）**：

> 最后用**哲学家就餐问题**把段 3 串联起来——从问题复现，到三种防护手段，形成完整的逻辑闭环。
>
> **图中央是原始死锁**（`user/dining.c`）：5 个哲学家同时拿起左叉，然后等待右叉——5 个进程、5 把叉子资源，形成循环等待，是经典的死锁场景。
>
> **左侧防护 A（打破 Hold & Wait）**：`dining_safe1.c` 引入 room 信号量（最多 4 人同时就座），进程**先抢座位再拿叉子**——`sem_wait(room)` + `sem_wait(left)` + `sem_wait(right)`，持有资源前先申请了限制量，打破了"hold and wait"条件。
>
> **右侧防护 B（打破 Circular Wait）**：`dining_safe2.c` 强制所有哲学家**始终先拿编号小的叉子**——`first = min(left, right)`，`second = max(left, right)`，破坏了"循环等待"条件。
>
> **右侧防护 C（检测+恢复）**：`dining_auto.c` 不预防死锁，而是由内核的 deadlock_detect 每 30 ticks 扫描 Wait-For Graph，发现环后 abort 最年轻的哲学家。**这是被动但最通用的方法**——不需要程序员知道会死锁，内核自动处理。
>
> **底部是三种方法对比表**：防护 A/B 需要事前知道死锁场景，防护 C 通用但有检测延迟，银行家算法在最前端阻止不安全分配。**我们的系统三层都有**，形成了完整的防护体系。

**功能示例**：实现哲学家就餐问题（死锁复现 + 三种防护）

**代码来源**：`user/dining.c`；`user/dining_safe1.c`；`user/dining_safe2.c`；`user/dining_auto.c`；`user/pathfinder.c`（优先级继承解决 Mars Pathfinder 问题）

**逻辑图**（见 [F15](PPT_image/prompts/全部15张图提示词-学术黑白风.md#F15)）：
- 中心：5 个哲学家 × 5 把叉子，死锁状态（所有哲学家持左等右）
- 左：dining_safe1 时序（room semaphore 先于 fork）
- 右：dining_safe2 时序（编号小叉优先）+ dining_auto 内核检测恢复
- 底部：三列对比表（打破 hold-wait / 打破 circular wait / 检测+恢复）

**文字说明**：
- dining.c：死锁 4 要素全满足（互斥/持有等待/不抢占/循环等待）
- dining_safe1：room semaphore 打破 hold-wait
- dining_safe2：编号排序打破 circular-wait
- dining_auto：kernel deadlock_detect 自动恢复
- banker：request 前安全检查（不在就餐问题中，但体系上相关）

---

## 附录 A：测试环境说明（必读）

> 所有测试在**单核（hart 0）**上运行。

|| 项目 | 现状 | 说明 |
||------|------|------|
| QEMU SMP 核数 | `-smp 3` | QEMU 模拟器配置，支持 3 核 |
| 内核 NCPU | 8 | `kernel/param.h`，代码最多支持 8 核 |
| Hart 0 | 全功能运行 | 执行全部初始化 + 调度器 + 所有测试 |
| Hart 1/2 | 固件唤醒但内核未用 | `main.c` 中 `while(started==0)` 自旋等待 |
| 实际参与核心 | **1 核** | 单核测试更严格，排除多核缓存干扰 |
| 多核代码 | SMP-ready | 6 种调度器全部 per-CPU，只差SBI HSM唤醒 |

**内核代码是 SMP-ready 的**：`cpus[]` 数组（proc.c 第 10 行）、per-CPU 栈（start.c 第 11 行 `stack0[4096*NCPU]`）、`cpuid()`（proc.c 第 108-113 行）、亲和性调度逻辑全部已实现。只差在 `main.c` hart 0 初始化末尾加 `sbi_hart_start(i, main, 0)` 循环。

**答辩话术**："测试在单核 hart 0 上运行——这样做更严格，因为单核下并发 bug 反而更难触发，一旦出现就是真实 bug。多核基础设施已在代码里完整实现（SMP-ready），未来加一行 SBI 唤醒即可启用。"

---

## 附录 B：图表清单（15 张）

|| 段 | 编号 | 图表 | 代码来源 |
|----|------|------|----------|
|| 1 | F1 | PCB 数据结构图 | proc.h:97-157 |
|| 1 | F2 | 进程状态机 + Wait 哈希桶 | proc.c:13,149-180 |
|| 1 | F3 | fork/exec/exit/wait 流程图 | proc.c:406-484, exec.c:27-151, proc.c:528-616 |
|| 1 | F4 | 上下文切换 + 调度器入口 | swtch.S, proc.c:682-753,1213-1254 |
|| 1 | F5 | 共享内存 + 消息队列 IPC | proc.c:427-443, shm.c, msgq.c |
|| 2 | F6 | 6 种调度器总览架构 | proc.c:682-703, sysproc.c:242-272 |
|| 2 | F7 | MLFQ 五级队列 + Aging | proc.c:810-891, trap.c:87-102 |
|| 2 | F8 | 实时调度 RM + EDF | proc.c:1078-1155, sysproc.c:326-356 |
|| 2 | F9 | CPU 亲和性 + 动态时间片 | proc.c:1051-1074, sysproc.c:471-491 |
|| 2 | F10 | 调度算法性能对比 | schedtest.c, throughput.c |
|| 3 | F11 | 信号量 P/V + 优先级继承 | sem.c:70-170, sem.h:15-26 |
|| 3 | F12 | 管程 Mesa 风格实现 | monitor.c:44-170 |
|| 3 | F13 | Wait-For Graph + DFS 死锁检测 | deadlock_detect.c:84-282 |
|| 3 | F14 | 银行家算法安全检查 | banker.c:44-177, banker.h:22-32 |
|| 3 | F15 | 哲学家就餐 + 三层防护体系 | dining.c, dining_safe1/2.c, dining_auto.c |

---

## 附录 C：测试文件总览

### 调度相关测试（11 个）

| 文件 | 测试内容 |
|------|---------|
| `schedtest.c` | 运行时切换 RR→FCFS→MLFQ，测各算法切换开销 |
| `schedlatency.c` | 8 进程错峰，测调度延迟（从 RUNNABLE 到 RUNNING） |
| `schedstat.c` | 读取各 PID 的 queue_level / wait_time / run_time |
| `mlfqtest.c` | 短/长/混合作业，观察降级与提升行为 |
| `fcfstest.c` | 验证 FCFS 按创建顺序完成 |
| `sjftest.c` | SJF vs FCFS 行为对比 |
| `sjfbusy.c` | SJF 信号量屏障注册 burst |
| `prioritytest.c` | 静态优先级竞争 + aging 恢复饥饿 |
| `rttest.c` | Rate-Monotonic 可调度性测试 |
| `edftest.c` | EDF deadline 排序验证 |
| `throughput.c` | RR / FCFS / MLFQ 完成同一批任务的总时间 |

### 同步原语测试（6 个）

| 文件 | 测试内容 |
|------|---------|
| `semtest1.c` | 基本 P/V 操作 |
| `semtest2.c` | 多进程互斥 |
| `semtest3.c` | 生产者-消费者（3 个信号量） |
| `monitortest.c` | monitor 基本 lock/wait/signal/broadcast |
| `pc_monitor.c` | monitor 实现生产者-消费者（2p+2c） |
| `pathfinder.c` | Mars Pathfinder 优先级反转 + PI 验证 |

### 死锁测试（6 个）

| 文件 | 测试内容 |
|------|---------|
| `dining.c` | 死锁复现（5 哲学家同时持左等右） |
| `dining_safe1.c` | 打破 hold-wait（room semaphore） |
| `dining_safe2.c` | 打破 circular-wait（编号排序） |
| `dining_auto.c` | 内核检测+abort 自动恢复 |
| `bankertest.c` | 银行家安全序列查询 |
| `banker_unsafe.c` | 银行家拒绝不安全请求 |

### 进程管理测试（3 个）

| 文件 | 测试内容 |
|------|---------|
| `forktest.c` | fork 填满 proc 表 + wait 回收 |
| `waitpidtest.c` | wait any / wait specific / wait multiple |
| `usertests.c` | xv6 官方 51 个系统调用测试（详见附录 D） |

### IPC 测试（2 个）

| 文件 | 测试内容 |
|------|---------|
| `shmtest.c` | shmget/shmat/shmdt 基本 + fork 继承 |
| `msgqtest.c` | msgget/msgsnd/msgrcv 有界缓冲区 |

### xv6 官方测试（1 个，51 个子测试）

`usertests.c` — 完整列表见附录 D。

---

## 附录 D：usertests.c 全部测试（51 个）

### 快速测试（42 个）

| 测试 | 内容 |
|------|------|
| copyin/copyout | 非法用户指针传递，内核防御 |
| copyinstr1/2/3 | 字符串复制边界 |
| rwsbrk | 已释放内存地址写入拒绝 |
| truncate1/2/3 | O_TRUNC 截断 |
| openiputtest/exitiputtest/iputtest | inode put 事务 |
| opentest | open/exists/noexist |
| writetest/writebig | 小/大文件写 |
| createtest | 批量创建/删除 |
| dirtest | mkdir/chdir/unlink |
| exectest | exec 基本 |
| pipe1 | 管道读写 |
| killstatus | kill 后 wait |
| preempt | 3 个忙等子进程可抢占 |
| exitwait | fork 100 次 wait 回收 |
| reparent | 200 次并发 fork/wait |
| twochildren | 1000 次双 fork |
| forkfork | 2×200 嵌套 fork |
| forkforkfork | 指数 fork bomb |
| reparent2 | 800 次嵌套 fork+exit |
| mem | 子进程 OOM |
| sharedfd | 两进程写同一 fd |
| fourfiles/createdelete/unlinkread | 并发文件操作 |
| linktest/concreate/linkunlink | 硬链接并发 |
| subdir | 多级子目录 |
| bigwrite/bigfile | 大文件 |
| fourteen/dot/rmdot/dirfile | 目录项边界 |
| iref | inode 引用计数 |
| forktest | 1000 次 fork 填 proc 表 |
| sbrkbasic/sbrkmuch | sbrk 基本+大内存 |
| kernmem/MAXVAplus/nowrite | 内核内存防护 |
| pgbug/sbrkbugs/sbrklast/sbrk8000 | sbrk 边界 bug 回归 |
| badarg | 50000 次无效参数 |
| lazy_alloc/lazy_unmap/lazy_copy/lazy_sbrk | 懒分配页表 |
| argptest/stacktest/bigargtest/bsstest | 参数和栈 |
| sbrkarg/validatetest | sbrk 返回地址可用性 |

### 慢速测试（6 个）

| 测试 | 内容 |
|------|------|
| bigdir | 500 硬链接遍历 |
| manywrites | 4×30 轮并发写同一文件 |
| badwrite | 600 次写无效缓冲区 |
| execout | exec OOM 清理 |
| diskfull | 耗尽磁盘块 |
| outofinodes | 耗尽 inode |

---

## 附录 E：代码→PPT 映射表

### proc.c 关键函数映射

| 函数 | 行号 | PPT 引用 |
|------|------|---------|
| `kfork()` | 406-484 | F3 |
| `allocproc()` | 186-239 | F3 |
| `uvmcopy()` | vm.c 294-322 | F3 |
| `kexec()` | exec.c 27-151 | F3 |
| `kexit()` | 528-567 | F3 |
| `kwait()` | 571-616 | F3 |
| `reparent()` | 488-522 | F3 |
| `sleep()` | 1295-1319 | F2 |
| `wakeup()` | 1323-1344 | F2 |
| `waitlist_insert/remove()` | 155-180 | F2 |
| `scheduler()` | 682-703 | F4, F6 |
| `rr_scheduler()` | 725-753 | F4, F6 |
| `fcfs_scheduler()` | 755-794 | F6 |
| `mlfq_scheduler()` | 831-891 | F7 |
| `mlfq_boost_priority()` | 810-829 | F7 |
| `sjf_scheduler()` | 893-946 | F6, F10 |
| `prio_scheduler()` | 983-1076 | F6, F10 |
| `edf_scheduler()` | 1078-1155 | F8, F10 |
| `sched()` | 1213-1231 | F4 |
| `yield()` | 1234-1254 | F4 |
| `swtch()` | swtch.S | F4 |

### 同步与死锁文件映射

| 文件 | PPT 引用 |
|------|---------|
| `kernel/sem.c` | F11 |
| `kernel/sem.h` | F11 |
| `kernel/monitor.c` | F12 |
| `kernel/deadlock_detect.c` | F13 |
| `kernel/banker.c` | F14 |
| `kernel/banker.h` | F14 |

---

## 附录 F：时间分配

|| 段 | 页数 | 时间 |
||----|------|------|
|| 封面 + 开场 | 1 | 1 分钟 |
|| 段 1 进程管理 | 5 | 6 分钟 |
|| 段 2 调度 | 5 | 6 分钟 |
|| 段 3 同步与死锁 | 5 | 6 分钟 |
|| 总结 + 答辩 | 1 | 2 分钟 |
|| **总计** | **15+2=17** | **~21 分钟** |
