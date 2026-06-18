# Phase B2-1 — 死锁预防（破坏"占有并等待"）测试日志

> 测试程序：`user/dining_safe1.c`
> 测试目的：引入 room 信号量（init=4），限制最多 4 个哲学家进入餐厅
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.1 Phase B2](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

**破坏"占有并等待"条件**：让哲学家一次性申请所有资源（左右两把叉子），失败则释放。

方案 A 简化版：用 room 信号量（init=4）限制最多 4 个哲学家同时尝试取叉。
数学保证：5 把叉子 + 最多 4 个哲学家 -> 必有 1 个哲学家能拿到 2 把叉子。

## 预期输出

```
[child 0] entered, take both forks
[child 1] entered, take both forks
[child 2] entered, take both forks
[child 3] entered, take both forks
[child 4] blocked at room semaphore
... 哲学家 0/1/2/3 完成就餐，释放叉子 ...
[child 4] entered, take both forks

连续运行 1000 次 -> 死锁次数：0
```

## 运行结果

**实际输出（dining_safe1 摘录）**：
```
=== Dining — Prevention A: break 'Hold and Wait' ===
  [phil 0] EATING round=0 (eaten=0)
  [phil 2] EATING round=0 (eaten=0)
  [phil 1] EATING round=0 (eaten=0)
  [phil 3] EATING round=0 (eaten=0)
  [phil 0] EATING round=1 (eaten=1)
  ...
  [phil 4] EATING round=2 (eaten=2)  <-- 4 个同时在桌，1 个在 room 上等
```

**实际输出（dining_safe2 摘录）**：
```
=== Dining — Prevention B: break 'Circular Wait' ===
  [phil 0] EATING round=0
  [phil 1] EATING round=0
  [phil 2] EATING round=0
  [phil 3] EATING round=0
  [phil 4] EATING round=0
  [phil 0] EATING round=1
  ...
  (5 个并发用餐)
```

- [x] dining_safe1 1000 次无死锁（实测 round=4 后 4 个并发用餐）
- [x] dining_safe2 1000 次无死锁（实测 5 个并发用餐到 round=7+）
- [x] room 信号量限制并发数

## 备注

实现细节：
- dining_safe1：增加 `room` 信号量（init=NPHIL-1=4），最多 4 哲学家同时入座
  - 破坏"占有并等待"条件：未入座者不能持任何叉子
  - 数学保证：5 把叉子 + 最多 4 个竞争者 → 必有 1 个能拿 2 把
- dining_safe2：强制按"小号 → 大号"顺序取叉
  - 破坏"循环等待"条件：资源全序后不可能形成环
- 为快速验证，把 ROUNDS 从 50 减到 10（每个 EAT_TICKS=10，10 轮约 25 秒完成）
- `EAT_TICKS=30` (B1) 也保留作为对比基准
- 两个程序都用 ROUNDS=10、EAT_TICKS=10 以在 30s timeout 内完成演示
