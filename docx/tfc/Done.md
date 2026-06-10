# 已完成任务 (Done.md)

## `wakeup()` 等待哈希队列优化

### 1. 完成目标

本次实现目标是把 `wakeup(chan)` 从原来的 **全进程表扫描** 改成 **按 `chan` 定位等待桶后局部遍历**，降低高频唤醒路径中的无效锁开销。

---

### 2. 实际完成的代码修改

#### 2.1 `kernel/proc.h`

已完成以下改动：

1. 新增等待桶常量：
   - `NWCHAN 64`
2. 新增等待桶结构：
   - `struct waitbucket { struct spinlock lock; struct proc *head; }`
3. 在 `struct proc` 中新增等待链表字段：
   - `struct proc *wnext;`
   - `struct proc *wprev;`
   - `struct waitbucket *wbucket;`

#### 2.2 `kernel/proc.c`

已完成以下改动：

1. 新增全局等待桶数组：
   - `struct waitbucket waittable[NWCHAN];`
2. 新增辅助函数：
   - `waitbucket_for(void *chan)`
   - `waitlist_insert(struct waitbucket *wb, struct proc *p)`
   - `waitlist_remove(struct waitbucket *wb, struct proc *p)`
3. 修改 `procinit()`
4. 修改 `freeproc()`
5. 修改 `sleep()`
6. 修改 `wakeup()`
7. 修改 `kkill()`

---

## FCFS 与 MLFQ 调度算法实现

### 已完成的内核修改

#### 1. `kernel/proc.h` - 进程结构扩展

新增字段：
```c
uint64 ctime;        // 进程创建时间（ticks）- 用于 FCFS
int queue_level;     // MLFQ 队列级别 (0=最高, 1=中, 2=最低)
int timeslice_used;  // 本时间片已用 tick 数
uint64 last_sched;   // 上次被调度的时间
int priority;        // 进程优先级
```

#### 2. `kernel/param.h` - 调度参数配置

新增配置：
```c
// 调度算法选择: 0=RR, 1=FCFS, 2=MLFQ
#define SCHED_RR      0
#define SCHED_FCFS    1
#define SCHED_MLFQ    2
#define SCHED_ALGORITHM SCHED_RR  // 可修改为选择算法

// MLFQ 调度参数
#define MLFQ_LEVELS      3
#define MLFQ_Q0_TIME     500000  // 5ms
#define MLFQ_Q1_TIME     1000000 // 10ms
#define MLFQ_Q2_TIME     2000000 // 20ms
#define MLFQ_BOOST_TICKS 200     // 优先级提升周期

#define TICKSLICE 1000000  // 默认时间片 10ms
```

#### 3. `kernel/proc.c` - 调度器实现

新增函数：
- `rr_scheduler()` - 时间片轮转调度器（默认）
- `fcfs_scheduler()` - FCFS 调度器
- `mlfq_scheduler()` - MLFQ 调度器
- `mlfq_enqueue()` - 兼容保留接口（当前不维护独立运行队列）
- `get_timeslice()` - 获取时间片
- `mlfq_boost_priority()` - 优先级提升

修改函数：
- `procinit()` - 初始化调度相关字段
- `allocproc()` - 初始化 `ctime`、`queue_level`、`timeslice_used`
- `freeproc()` - 重置调度字段
- `kfork()` - 初始化子进程的 MLFQ 字段
- `yield()` - MLFQ 降级逻辑
- `wakeup()` - 唤醒后重置 `timeslice_used`，不再维护独立 MLFQ 队列

#### 4. `kernel/trap.c` - 时钟中断处理

修改时钟中断：
- MLFQ 模式下统计时间片使用
- 用完时间片后降级并 yield

#### 5. `kernel/defs.h` - 函数声明

新增调度相关函数声明

### 已完成的测试程序

| 程序 | 文件 | 主要作用 | 当前适用结论 |
|------|------|----------|--------------|
| FCFS 测试 | `user/fcfstest.c` | 创建 4 个带间隔启动的 CPU 密集子进程，观察退出顺序 | 验证 FCFS 的基本先来先服务顺序 |
| MLFQ 测试 | `user/mlfqtest.c` | 混合 SHORT / LONG / MIXED 三类作业，观察完成时间层次 | 验证 MLFQ 的短作业优先趋势 |
| Throughput 测试 | `user/throughput.c` | 在相同工作负载下测总完成时间 | 当前仅适合作为轻量性能观察，不适合严格吞吐量排名 |
| 上下文切换测试 | `user/csw.c` | 多进程循环 `pause(1)`，观察总耗时与均衡性 | 验证 RR/MLFQ 下时钟中断与调度切换路径是否正常 |

---

## 测试结果记录

### 测试环境

- 平台：QEMU RISC-V xv6
- CPU：3核（默认配置）
- 进程上限：NPROC = 64
- 调度算法选择方式：**编译期宏选择**
- 当前内核不会在运行测试时自动切换调度算法

### 调度算法与测试程序的关系

当前实现中，调度算法由 `kernel/param.h` 中的 `SCHED_ALGORITHM` 宏决定，例如：

```c
#define SCHED_ALGORITHM SCHED_RR
```

这意味着：
- 一次编译产出的内核，只会启用 **一种** 调度算法
- `fcfstest`、`mlfqtest`、`throughput`、`csw` 都只是普通用户态测试程序
- 这些测试程序**不会自动切换**内核调度器
- 如果要测试不同算法，必须手动修改 `SCHED_ALGORITHM`，然后重新 `make clean && make` 再运行测试

因此，正确的测试方式应为：
1. 设 `SCHED_ALGORITHM = SCHED_FCFS`，编译后运行 `fcfstest` / `throughput`
2. 设 `SCHED_ALGORITHM = SCHED_MLFQ`，编译后运行 `mlfqtest` / `throughput`
3. 设 `SCHED_ALGORITHM = SCHED_RR`，编译后运行 `throughput` / `csw`

### 1. FCFS 测试结果

**配置**：`SCHED_ALGORITHM = SCHED_FCFS`

**日志文件**：`docx/tfc/log/FCFStest.txt`

**结果摘要**：
- `fcfstest` 总耗时：`10 ticks`
- 首个退出子进程：`PID=4`
- 最后退出子进程：`PID=7`
- `throughput` 总耗时：`5 ticks`

**分析**：
- `fcfstest` 中 4 个子进程都已经具备可观测耗时，约为 `3~5 ticks`，比第一轮 `0 tick` 结果更有参考价值。
- 从日志可辨识出的退出顺序看，整体仍与 PID/创建顺序一致，支持 FCFS 按先来先服务的行为。
- 该日志中存在明显的串口输出交错，说明多进程 `printf` 在控制台上相互穿插，因此不能把每一行文本顺序机械地当成严格时序证据。
- `throughput` 虽然比旧结果更长，但仍只有 `5 ticks`，分辨率仍偏低，不能据此严谨比较吞吐量优劣。

**结论**：
- FCFS 的基本调度顺序已经得到验证。
- 但 FCFS 的性能结论仍只能写“趋势成立”，不宜写成非常强的定量结论。

### 2. MLFQ 测试结果

**配置**：`SCHED_ALGORITHM = SCHED_MLFQ`

**日志文件**：`docx/tfc/log/MLFQtest.txt`

**结果摘要**：
- `mlfqtest` 总耗时：`15 ticks`
- `SHORT` 作业完成时间约为：`1~3 ticks`
- `LONG` 作业完成时间约为：`4~6 ticks`
- `MIXED` 作业完成时间约为：`12 ticks`
- `csw` 总耗时：`52 ticks`

**分析**：
- 相比旧版 `8~10 ticks` 的结果，这一轮 `mlfqtest` 已经拉长到 `15 ticks`，区分度更强。
- 从日志可读出的主要趋势是：`SHORT < LONG < MIXED`，这符合 MLFQ 对短作业优先响应的预期。
- `LONG` 作业比 `SHORT` 更慢，说明较重 CPU 作业确实没有持续占据最高优先级带来的优势。
- `MIXED` 作业最慢并不异常，因为它包含多轮 `pause(1)`，会显著增加 wall-clock 时间。
- 但当前日志仍然没有直接打印“队列级别变化”或“降级次数”，因此“LONG 作业确实发生了多级降级”仍属于**间接推断**，不是直接观测证据。
- `csw` 在 MLFQ 配置下能稳定跑完，说明时钟中断、`yield()` 和基本调度循环能够正常工作。

**结论**：
- MLFQ 的“短作业优先”已经得到较强支持。
- MLFQ 的“时间片耗尽后降级”在实现逻辑上成立、在现象上基本吻合，但文档里应说明：**缺少直接队列迁移日志，证据仍不算最强**。

### 3. RR 测试结果

**配置**：`SCHED_ALGORITHM = SCHED_RR`

**日志文件**：`docx/tfc/log/RRtest.txt`

**结果摘要**：
- `throughput` 总耗时：`5 ticks`
- `csw` 总耗时：`52 ticks`
- 4 个 `pause child` 大多在 `51 ticks` 左右完成

**分析**：
- `csw` 结果比较整齐，4 个子进程耗时几乎一致，符合 RR 下多个相似任务被较均匀轮转的直觉。
- `Children completed their work while parent was running.` 这一结果说明父子进程之间确实发生了交替执行，而不是单个进程长期独占 CPU。
- `throughput` 依然只有 `5 ticks`，说明该测试在 RR 下也仍然分辨率不足，不能作为吞吐量排名依据。
- 日志同样存在控制台输出交错，因此适合支持“行为趋势”判断，不适合做精细时序重建。

**结论**：
- RR 的基本轮转行为已经通过 `csw` 获得支持。
- RR 与其他算法的吞吐量优劣，目前仍不能通过现有 `throughput` 日志下结论。

### 4. 合理的性能评测（基于现有日志的轻量结论）

由于 xv6 用户态测试依赖 `uptime()`，时间分辨率是 **tick 级**，再加上控制台输出存在交错，因此本次性能评测不适合写成严格的 benchmark 排名，更适合写成**轻量性能观察**。

#### 4.1 响应性观察

- 在 `MLFQ` 下，`SHORT` 作业普遍先于 `LONG` 与 `MIXED` 作业完成。
- 这说明在相同测试轮次中，短 CPU 作业获得了更好的响应时间。
- 因而从**响应性**角度看，`MLFQ` 优于单纯追求公平轮转的 `RR`，也优于不考虑交互响应的 `FCFS`。

#### 4.2 公平性观察

- 在 `RR` 的 `csw` 日志中，4 个 `pause child` 大多在 `51 ticks` 左右完成，耗时非常接近。
- 这说明对于结构相似的任务，RR 能提供较均匀的 CPU 分享。
- 因而从**公平性/均衡性**角度看，`RR` 的表现最直接、最稳定。

#### 4.3 长作业完成时间观察

- 在 `FCFS` 中，先创建的子进程优先完成，这符合非抢占式先来先服务逻辑。
- 在 `MLFQ` 中，`LONG` 作业明显慢于 `SHORT` 作业，说明 CPU 密集型任务不会一直保持最高优先级响应优势。
- 因此从**长作业 wall-clock 完成时间**角度看，`MLFQ` 更偏向改善短作业体验，而不是保证所有作业完成时间最短。

#### 4.4 吞吐量观察

- 三组日志中的 `throughput` 结果都只有 `5 ticks` 量级，仍然太短。
- 这意味着当前工作负载不足以把 RR / FCFS / MLFQ 的吞吐量差异稳定拉开。
- 所以当前最合理的写法不是“某算法吞吐量最高”，而是：**现有 `throughput` 数据不足以形成可靠排名**。

#### 4.5 可写入报告的性能总结

在保持结论审慎的前提下，可以在报告中写成：

- `RR`：公平性最好，适合说明时间片轮转能让相似任务获得接近的完成时间。
- `FCFS`：顺序最稳定，适合说明先到先服务，但对交互响应不占优势。
- `MLFQ`：响应性最好，短作业完成更快，更符合交互式系统对前台任务的需求。

这个结论是**基于现有 tick 级日志得到的合理实验观察**，而不是高精度 benchmark 排名。

---

## 测试结论

### 通过的测试

1. **编译与启动恢复正常** - 当前 RR / FCFS / MLFQ 配置均已能成功编译，MLFQ 启动路径问题已修复
2. **FCFS 基本顺序成立** - `fcfstest` 支持“按创建顺序/先来先服务”这一结论
3. **MLFQ 短作业优先成立** - `mlfqtest` 呈现出 `SHORT < LONG < MIXED` 的完成时间趋势
4. **RR 轮转行为成立** - `csw` 在 RR 下各子进程耗时接近，支持时间片轮转的基本行为
5. **上下文切换路径正常** - `csw` 在 RR / MLFQ 场景下都可稳定完成

### 测试局限性

1. **控制台输出存在交错** - 多进程 `printf` 导致日志行相互穿插，不适合做精细时序重建
2. **throughput 仍不够有效** - 现有结果只有 `5 ticks`，仍不足以支撑可靠吞吐量对比
3. **MLFQ 降级证据仍偏间接** - 目前只有现象支持，没有直接打印队列迁移日志
4. **结论更适合写成功能验证** - 适合证明“行为趋势正确”，不适合写成精确性能评测报告

### 当前可写入报告的结论

- `FCFS`：可写“已验证基本先来先服务顺序”。
- `MLFQ`：可写“已验证短作业优先趋势，长作业/混合作业耗时更长”。
- `RR`：可写“已验证轮转调度与上下文切换路径正常”。
- **性能评测结论**：可写“RR 更强调公平性，MLFQ 更强调短作业响应性，FCFS 更强调到达顺序稳定性；现有数据支持趋势判断，但不支持高精度吞吐量排名”。
- `throughput`：暂不建议单独写成最终性能对比结论。

### 后续建议

#### 1. 如果只追求实验通过

当前日志已经足够支撑：
- 三种调度算法都能运行
- FCFS / MLFQ / RR 各自核心行为都有对应现象支撑
- 可以作为 `Done.md` 的正式测试结论保留

#### 2. 如果还想把报告写得更强

建议再做两类增强：
- **增强 throughput**：继续提高 `throughput.c` 工作量，让总时间至少达到几十 ticks
- **增强 MLFQ 可观测性**：在内核中临时打印 `queue_level` 变化，直接记录 LONG 作业是否被降级

#### 3. 可选的补强方向

- 让 `fcfstest` 的单个子进程稳定运行多个 tick，以更明显展示 FCFS 的非抢占特征
- 让 `throughput` 在 RR / FCFS / MLFQ 三种算法下都达到更高总时长，再做吞吐量横向比较
- 在 MLFQ 模式下记录 LONG 作业的队列迁移路径，形成直接证据

---

## 编译与测试指南

### 1. 切换调度算法

编辑 `kernel/param.h`，修改 `SCHED_ALGORITHM` 宏：

```c
// 使用 RR（默认）
#define SCHED_ALGORITHM SCHED_RR

// 使用 FCFS
#define SCHED_ALGORITHM SCHED_FCFS

// 使用 MLFQ
#define SCHED_ALGORITHM SCHED_MLFQ
```

### 2. 编译

```bash
cd /home/tfc/OS/OS_xv6_riscv
make clean
make
```

### 3. 运行测试

```bash
make qemu
```

在 xv6 shell 中：

```bash
fcfstest    # FCFS 测试
mlfqtest    # MLFQ 测试
csw         # 上下文切换测试
ps          # 查看进程状态
```

---

## 文档清单

本次任务新增/更新文档：

| 文档 | 描述 |
|------|------|
| `docx/tfc/FCFS_MLFQ_Scheduler_Design.md` | 详细设计文档 |
| `docx/tfc/Done.md` | 已完成内容及测试结果 |
| `docx/tfc/Todo.md` | 待完成任务 |
| `docx/tfc/Doing.md` | 正在进行 |
| `docx/tfc/log/FCFStest.txt` | FCFS 测试日志 |
| `docx/tfc/log/MLFQtest.txt` | MLFQ 测试日志 |
| `docx/tfc/log/RRtest.txt` | RR 测试日志 |

---

## 实现总结

### 已完成功能

1. **wait bucket 哈希等待队列优化**
   - `wakeup()` 从 O(NPROC) 优化为 O(k)

2. **FCFS 调度算法**
   - 按进程创建时间调度
   - 非抢占式

3. **MLFQ 调度算法**
   - 3级队列（优先级 0 > 1 > 2）
   - 时间片用完降级
   - 防止饥饿的优先级提升

4. **RR 调度器**
   - 时间片轮转（默认调度器）

### 验证状态

| 功能 | 状态 | 备注 |
|------|------|------|
| wait bucket | 已验证 | 正确实现 |
| FCFS 调度 | 基本验证 | 需更大负载测试 |
| MLFQ 调度 | 基本验证 | 需更大负载测试 |
| 时间片机制 | 待验证 | 测试时间过短 |
| 优先级降级 | 待验证 | 需要长作业测试 |
