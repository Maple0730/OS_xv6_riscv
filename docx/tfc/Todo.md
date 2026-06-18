# 待完成任务 (Todo.md)

## 进程管理和处理器调度

---

## 已完成

### 信号量机制实现

#### 内核修改

- `kernel/sem.h` - 信号量数据结构定义
- `kernel/sem.c` - 内核信号量函数实现 (sem_init, sem_wait, sem_post, sem_get, sem_close)
- `kernel/main.c` - 添加 seminit() 调用
- `kernel/defs.h` - 添加信号量函数声明
- `kernel/syscall.h` - 添加系统调用编号 (SYS_sem_open ~ SYS_sem_close)
- `kernel/sysproc.c` - 实现信号量系统调用入口
- `kernel/syscall.c` - 添加分发条目
- `Makefile` - 添加 sem.o 编译目标

#### 用户态接口

- `user/sem.h` - 用户API头文件
- `user/sem.c` - 用户态包装函数
- `user/usys.pl` - 添加桩代码生成

#### 测试程序

- `user/semtest1.c` - 基本 P/V 操作测试
- `user/semtest2.c` - 互斥锁测试
- `user/semtest3.c` - 生产者-消费者测试

### waitpid 系统调用实现

#### 内核修改

- `kernel/proc.c` - 添加 `kwaitpid(int pid, uint64 addr)` 函数
- `kernel/sysproc.c` - 添加 `sys_waitpid()` 系统调用入口
- `kernel/defs.h` - 添加 `kwaitpid` 函数声明
- `kernel/syscall.h` - 添加系统调用编号 `SYS_waitpid` (33)
- `kernel/syscall.c` - 添加分发条目

#### 用户态接口

- `user/user.h` - 添加 `waitpid(int pid, int *status)` 函数原型
- `user/usys.pl` - 添加桩代码生成
- `Makefile` - 添加 `_waitpidtest` 编译目标

#### 测试程序

- `user/waitpidtest.c` - waitpid 功能测试

### 共享内存机制完善

#### 内核修改

- `kernel/shm.h` - 扩展 `struct shm`（新增 forkcount 字段）
- `kernel/shm.c` - 完整实现 shmget / shmat / shmdt（含引用计数机制）
- `kernel/proc.c` - kfork 复制共享内存映射；freeproc 递减引用计数
- `kernel/vm.c` - uvmunmap 保护共享内存页面不被错误释放
- `kernel/proc.h` - 添加 `shm_shmidx`、`rqnext`/`rqprev`、统计字段
- `kernel/sysproc.c` - sys_shmget / sys_shmat / sys_shmdt 实现
- `kernel/syscall.h` - SYS_shmget=29, SYS_shmat=30, SYS_shmdt=31

#### 用户态接口

- `user/user.h` - shmget / shmat / shmdt 用户 API（含 IPC_CREAT 宏）
- `user/usys.pl` - 桩代码

#### 测试程序

- `user/shmtest.c` - 基础测试 / 父子通信测试 / fork 继承测试

### MLFQ 五级队列与调度统计

#### 内核修改

- `kernel/param.h` - MLFQ_LEVELS=5，新增 Q3/Q4 时间片常量
- `kernel/proc.c` - 5级MLFQ调度器（进程表扫描）、mlfq_boost_priority、调度统计收集
- `kernel/proc.h` - wait_time/run_time/sched_count（统计字段）
- `kernel/sysproc.c` - sys_schedstat 实现（SYS_schedstat=38）

#### 测试程序

- `user/schedstat.c` - 调度统计查看程序
- `user/schedlatency.c` - 调度延迟测试程序

### 高精度计时器

#### 内核修改

- `kernel/sysproc.c` - sys_cgettimeofday() 实现（SYS_cgettimeofday=37）

#### 测试程序

- `user/cgettime.c` - 微秒级计时器测试程序

### 性能测试增强

#### 测试程序

- `user/throughput.c` - 重写，三算法对比（RR/FCFS/MLFQ）
- `user/schedlatency.c` - 调度延迟测试（新增）

---

## 测试验证

### 编译与运行

```bash
cd /home/tfc/OS/OS_xv6_riscv
make clean && make
make qemu
```

### 在 xv6 shell 中运行测试
```bash
shmtest      # 共享内存测试
semtest1     # 基本信号量测试
semtest2     # 互斥锁测试
semtest3     # 生产者-消费者测试
waitpidtest  # waitpid 系统调用测试
mlfqtest     # MLFQ 调度观测测试
schedtest    # 调度算法切换测试
schedstat    # 调度统计查看
cgettime     # 高精度计时器测试
throughput   # 吞吐量对比测试
schedlatency # 调度延迟测试
ps           # 查看进程状态

# === 高级扩展 (Phase A-F) ===
dining         # 哲学家就餐死锁复现
dining_safe1   # 死锁预防方案1
dining_safe2   # 死锁预防方案2
bankertest     # 银行家算法
banker_unsafe  # 不安全请求演示
monitortest    # 管程基本测试
pc_monitor     # 管程实现的生产者-消费者
prioritytest   # 优先级调度 + aging
pathfinder     # Mars Pathfinder 优先级反转
rmtest         # Rate-Monotonic 实时调度
edftest        # EDF 实时调度
rttest         # 实时任务截止时间达成率
cpuaffinity    # Per-CPU 亲和性
msgqtest       # 消息队列 IPC
```

---

## 参考文档

- 设计文档：`docx/tfc/FCFS_MLFQ_Scheduler_Design.md`
- 已完成任务：`docx/tfc/Done.md`
- 信号量实现详情：见 `kernel/sem.c`
- waitpid 实现详情：见 `kernel/proc.c`
- 共享内存实现详情：见 `kernel/shm.c`
- **下一阶段规划**：`docx/tfc/ProcessMgmt_Scheduling_NextPhase.md`

---

## 下一阶段任务总览（2026-06-16 规划 — 高级扩展版）

> 规划原则：**理论体系完整 + 汇报效果突出 + 内核态真实现**
> 详细路线图：`docx/tfc/ProcessMgmt_Scheduling_AdvancedExt.md`
> 早一版规划（已被本版替代）：`docx/tfc/ProcessMgmt_Scheduling_NextPhase.md`

### S 级（必做 — 理论核心 + 汇报效果最佳）

**Phase B — 死锁专题（OS 课程最核心）**
- [x] B1 死锁复现（哲学家就餐，5 进程 5 叉子）— 见 `docx/tfc/log/dining.md`
- [x] B2 死锁预防（破坏占有并等待 / 破坏循环等待，2 种方案）
- [x] B3 银行家算法（系统调用 #40-42 + 安全性检查 + 5 进程 3 资源测试）
- [x] B4 死锁检测（等待图 DFS）+ 自动恢复

**Phase C — 管程 + 条件变量**
- [x] C1 管程 Monitor（系统调用 + 条件变量 wait/signal/broadcast）
- [x] C2 用管程重写生产者-消费者（与 semtest3 对比）— 见 `docx/tfc/log/pc_monitor.md`

**Phase A2 + D1 — 优先级调度 + 优先级继承**
- [x] A2 优先级调度（复用 priority 字段）+ aging 解决饥饿
- [x] A2 优先级继承（高优先级等待时提升持锁进程优先级）
- [x] D1 Mars Pathfinder Bug 复现 — 见 `docx/tfc/log/pathfinder.md`

### A 级（强烈推荐 — 与"完整理论版"目标对齐）

**Phase F — 实时调度（RM / EDF）**
- [x] F1 Rate-Monotonic（周期越短优先级越高）— 见 `docx/tfc/log/rmtest.md`
- [x] F2 Earliest Deadline First（截止时间最小堆）— 见 `docx/tfc/log/edftest.md`
- [x] F3 实时任务截止时间达成率测试 — 见 `docx/tfc/log/rttest.md`

**Phase E — 多核调度（NCPU=8 真正利用）**
- [x] E1 Per-CPU 调度队列重构（每 CPU 局部 runq）— 见 `docx/tfc/log/cpuaffinity.md`
  - 注：当前 QEMU xv6 启动只唤醒 hart 0，affinity 机制已就位但单核演示
- [x] E2 负载均衡 Pull 策略 — 跟随 E1 一起完成（受单核限制）
- [x] E3 多核同步原语压测（spinbench / mp_bal）— 受单核限制跳过

### B 级（可选 — 锦上添花）

- [x] D2 消息队列（sys_mq_send / mq_recv，环形队列）— 见 `docx/tfc/log/msgqtest.md`

**所有阶段均已完成（部分受单核 SMP 启动限制只能演示机制）**。

### 优先级与排期建议

按"汇报价值 × 理论重要性"综合排序：

1. **最优先**（直接对应教学核心 + 课堂讲解亮点）：B（死锁）、A2+D1（优先级反转）
2. **次优先**（与已有信号量并列的高级范式）：C（管程）
3. **再次**（高级扩展，理论加分项）：F（实时）、E（多核）
4. **可选**（时间充裕时做）：D2（消息队列）

### 详细路线图与测试计划

详见 [`docx/tfc/ProcessMgmt_Scheduling_AdvancedExt.md`](ProcessMgmt_Scheduling_AdvancedExt.md)：
- §2 各 Phase 详细设计（数据结构 / 系统调用 / 测试程序）
- §3 测试计划（测试矩阵 / 对比实验 / 性能基准）
- §4 排期建议（按 S/A/B 排序）
- §6 汇报演示脚本（5 段、约 60min）
- §7 验收标准（覆盖 11 个 OS 课程核心问题）
- §9 关键文件改动预期清单

### 早期规划归档

`docx/tfc/ProcessMgmt_Scheduling_NextPhase.md` 中：
- **A1（SJF）已实际实现并验证**（见 `Done.md` 1775-1788，`user/sjfbusy.c`），从本规划中移除
- 其他 A1-A3 / B1-B4 / C1-C2 / D1-D2 / E1-E3 / F1-F3 已按汇报价值重新组织为本版 S/A/B 三级结构
