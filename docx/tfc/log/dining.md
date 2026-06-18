# Phase B1 — 死锁复现实验（哲学家就餐）测试日志

> 测试程序：`user/dining.c`
> 测试目的：演示 5 个哲学家同时拿左叉后陷入循环等待，验证死锁的 4 个条件
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.1 Phase B1](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

死锁的 4 个必要条件：
1. 互斥条件：叉子（信号量）一次只能被一个哲学家持有
2. 占有并等待：哲学家拿到左叉后，等待右叉
3. 不可剥夺：叉子不能强制从哲学家手中抢走
4. 循环等待：P0 -> P1 -> P2 -> P3 -> P4 -> P0

5 个哲学家 + 5 把叉子 + 全部同时拿左叉 = 死锁。

## 预期输出

```
[child 0] PID=... take left fork (sem 0)
[child 1] PID=... take left fork (sem 1)
[child 2] PID=... take left fork (sem 2)
[child 3] PID=... take left fork (sem 3)
[child 4] PID=... take left fork (sem 4)
[child 0] PID=... try to take right fork (sem 1) -> BLOCKED
[child 1] PID=... try to take right fork (sem 2) -> BLOCKED
[child 2] PID=... try to take right fork (sem 3) -> BLOCKED
[child 3] PID=... try to take right fork (sem 4) -> BLOCKED
[child 4] PID=... try to take right fork (sem 0) -> BLOCKED

ps 输出:
  ... 5 sleeping ...

[DEADLOCK] 5 processes sleeping forever, no progress.
```

## 运行结果

**实际输出（节选）**：
```
Created 5 fork semaphores (init=1)

Forking 5 philosopher processes...
  [parent] forked phil 0, pid=4
  [parent] forked phil 1, pid=5
  [parent] forked phil 2, pid=6
  [parent] forked phil 3, pid=7
  [parent] forked phil 4, pid=8

  [phil 0] take LEFT  fork=0
  [phil 1] take LEFT  fork=1
  [phil 2] take LEFT  fork=2
  [phil 3] take LEFT  fork=3
  [phil 4] take LEFT  fork=4
  [phil 0] try  RIGHT fork=1
[sem_wait] pid=4 sem=1 value=0 -> sleeping
  [phil 1] try  RIGHT fork=2
[sem_wait] pid=5 sem=2 value=0 -> sleeping
  [phil 2] try  RIGHT fork=3
[sem_wait] pid=6 sem=3 value=0 -> sleeping
  [phil 3] try  RIGHT fork=4
[sem_wait] pid=7 sem=4 value=0 -> sleeping
  [phil 4] try  RIGHT fork=0
[sem_wait] pid=8 sem=0 value=0 -> sleeping
(qemu 因父进程 wait 永远阻塞而被 timeout 终止)
```

**对照预期**：完全符合 — 5 个进程全部进入 SLEEPING，无任何进程完成 1 轮用餐。

- [x] 死锁复现成功
- [x] 5 个进程全部 SLEEPING（`ps` 可见 5 个 sleep）
- [x] 父进程 wait 永远阻塞（qemu timeout 证实）

## 备注

实现细节：
- 5 个 `sem_open(1)` 模拟 5 把叉子
- 每个 philosopher: left = i, right = (i+1) % 5
- 取叉顺序固定 LEFT → RIGHT（故意触发循环等待）
- 5 个 child fork 后先用 `pause(5)` 对齐进度，保证 5 个进程几乎同时到 right fork 阻塞点
- 父进程 `wait(0)` 永远阻塞 — 用 qemu `timeout 25` 命令行强制终止演示
- 父进程退出后 init 重新拉起 sh；实验用 `Ctrl-P` 可在 SLEEPING 列表中看到 5 个死锁进程

B2 / B3 / B4 将在此基础上展示预防、避免、检测 3 种处理方式。
