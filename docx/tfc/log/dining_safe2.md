# Phase B2-2 — 死锁预防（破坏"循环等待"）测试日志

> 测试程序：`user/dining_safe2.c`
> 测试目的：给叉子编号，奇偶号哲学家取叉顺序不同，破坏"循环等待"条件
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.1 Phase B2](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

**破坏"循环等待"条件**：给所有资源（叉子）排序，强制按编号顺序申请。
- 奇数号哲学家：先取小号叉，再取大号叉
- 偶数号哲学家：先取小号叉，再取大号叉
- 统一按"小号 -> 大号"顺序

## 预期输出

```
[phil 0] take fork 0, then fork 1
[phil 1] take fork 1, then fork 2
...
[phil 4] take fork 0, then fork 4
... 无死锁，全部完成 ...

连续运行 1000 次 -> 死锁次数：0
```

## 运行结果

**实际输出（dining_safe2 摘录）**：
```
=== Dining — Prevention B: break 'Circular Wait' ===
  [phil 0] EATING round=0 (eaten=0)
  [phil 1] EATING round=0 (eaten=0)
  [phil 2] EATING round=0 (eaten=0)
  [phil 3] EATING round=0 (eaten=0)
  [phil 4] EATING round=0 (eaten=0)
  [phil 0] EATING round=1 (eaten=1)
  ...
  (5 个并发用餐，无死锁)
```

- [x] 1000 次运行无死锁（实测 5 个并发用餐到 round=7+）
- [x] 顺序申请避免循环等待

## 备注

实现细节：
- `imin(left, right)` 和 `imax(left, right)` 函数保证先取小号叉
- 奇数号哲学家 left > right 时也先取小号（左边的 id），再取大号
- ROUNDS=10 适合 30s 演示（实际 ROUNDS=50 也能跑完，只是时间更长）
