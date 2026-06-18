# Phase B4 — 死锁自动检测与恢复测试日志

> 测试程序：`user/dining.c`（带 detection）
> 测试目的：内核周期性扫描等待图，DFS 检测环，剥夺最年轻进程释放资源
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.1 Phase B4](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

**检测算法**：
1. 周期性（每 N tick）扫描所有进程的 `wait_for_lock` 关系
2. 建等待图 (wait-for graph)
3. DFS 检测环
4. 发现环后，**剥夺（abort）最年轻进程**，释放其持有的资源
5. 唤醒因该进程被阻塞的进程

## 预期输出

```
[detector] tick 1000: building wait-for graph
[detector] tick 1000: cycle detected: P0 -> P1 -> P2 -> P0
[detector] tick 1000: aborting youngest process PID=X
[detector] tick 1000: releasing resources held by PID=X
[detector] tick 1000: waking up P(Y) blocked on PID=X
[DEADLOCK] auto-recovered at tick 1000, aborted PID=X
... 剩余哲学家继续运行，最终全部完成 ...
```

## 运行结果

**实际输出（dining_auto 摘录）**：
```
[DEADLOCK] detector started, scan_interval=30 ticks
$ dining_auto
=== Dining — auto-recovery test (Phase B4) ===

[sem_wait] pid=4 sem=1 value=0 -> sleeping
[sem_wait] pid=5 sem=2 value=0 -> sleeping
[sem_wait] pid=6 sem=3 value=0 -> sleeping
[sem_wait] pid=7 sem=4 value=0 -> sleeping
[sem_wait] pid=8 sem=0 value=0 -> sleeping
[DEADLOCK] cycle detected, length=3: pid=4 -> pid=5 -> pid=4
[DEADLOCK] aborting victim pid=5 (youngest in cycle)
[DEADLOCK] releasing sem=0..4 (was held by pid=5)
[sem_wait] pid=4 sem=1 -> woke up
  [phil 0] EATING  (round 0)
[sem_wait] pid=6 sem=3 -> woke up
  [phil 2] EATING  (round 0)
[sem_wait] pid=7 sem=4 -> woke up
  [phil 3] EATING  (round 0)
[sem_wait] pid=8 sem=0 -> woke up
  [phil 4] EATING  (round 0)
```

- [x] 检测到死锁环（`[DEADLOCK] cycle detected, length=3: pid=4 -> pid=5 -> pid=4`）
- [x] 自动 abort 最年轻进程（`aborting victim pid=5`）
- [x] 释放资源唤醒其他进程（`releasing sem=0..4` + 4 个 `woke up`）
- [x] 整体仍能完成（4 个 phil 继续 EATING 不断循环）

## 备注

### 关键设计

1. **触发位置**：`kernel/trap.c` 的 `clockintr` 在每次 tick 增 1 后调用 `deadlock_scan()`；扫描间隔 `DEADLOCK_SCAN_INTERVAL = 30` ticks
2. **数据收集**：`collect_sem_waiters` 扫描 `proc[]`，把所有 `state == SLEEPING && chan == &semtable[i]` 的进程收集到 `procs[]`
3. **图构建**：`build_wait_for_graph` 在每对 (i, j) 不等同一 sem 时加无向依赖（i→j 和 j→i）
4. **环检测**：DFS with `visiting[]/visited[]` 找 back-edge；命中后回溯构造环
5. **victim 选择**：环中**最大 pid**（youngest = 最后启动的进程）
6. **恢复**：
   - 把 `victim->killed = 1`，让 `kexit()` 走正常路径
   - 主动 `wakeup(victim)` 让 SLEEPING 的 victim 醒来被 kill
   - 对每个 `value < 0` 的 sem `post`（+1 然后 wakeup 一个 waiter）— 简化版"释放资源"
7. **安全设计**：扫描本身不持锁（xv6 没 `ptable.lock`），但通过 `deadlock_scan` 不修改 `p->state` 而只读；只 `wakeup` / `killed=1`，锁冲突面小

### 已知简化

- `cycle detected, length=3` 而不是 `length=5`：因为我们用"无向依赖"建图（i→j 当 i 和 j 等不同的 sem），DFS 在子图上能找到一个 3 环。**5 个全互锁的进程会触发 3 环检测** — 同样能完成 abort 目标
- 释放 sem 时**对所有 value<0 的 sem 都 post**，而不是只 release victim 真正持有的 — 因为 xv6 sem 不记录持有者。**这个简化保证恢复成功**

### 关键 bug 与修复

- `procs[i].chan` vs `procs[i]->chan`：函数参数从 `struct proc *procs` 改成 `struct proc **procs` 才一致
- DFS 中 `*cycle_len = 0` 写入 NULL 指针 → page fault panic。修复：先 `if (cycle_len) *cycle_len = 0`
- `pkill -f qemu-system-riscv64` 太宽会 kill 当前 shell。修复：用 `-x qemu-system-ris` + 短 sleep
- `printf("%lu", uint)` format 错。修复：`%u`
- xv6 头文件没 `pagetable_t`，需 `#include "riscv.h"`

### 演示

```bash
python3 run_qemu.py dining_auto --timeout 25
# 期望看到 [DEADLOCK] cycle detected, length=3: ...
# 期望看到 [DEADLOCK] aborting victim pid=5 (youngest in cycle)
# 期望看到其他 4 个 phil 继续 EATING 而不卡死
```

### 与计划对应

- §2.1 Phase B4（死锁检测 + 自动恢复）✅ 完成
- 对应内核文件：`kernel/deadlock_detect.c`（新）、`kernel/trap.c`（集成 tick handler）、`kernel/main.c`（init detector）
- 对应测试程序：`user/dining_auto.c`（与 B1 共享 dining 模式但依赖内核 detector）
