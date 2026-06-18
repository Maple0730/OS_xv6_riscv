# Phase C — 管程 + 条件变量 测试日志

> 测试程序：`user/monitortest.c`
> 测试目的：3 个生产者 / 3 个消费者使用管程 + 条件变量，验证 Mesa 语义
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.1 Phase C](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

**管程（Mesa 风格）API**：

```c
mon_create(name)          // -> mid
mon_enter(mid)            // 互斥进入
cv_wait(mid, cvid)        // 释放管程锁并睡眠
cv_signal(mid, cvid)      // 唤醒一个等待者
cv_broadcast(mid, cvid)   // 唤醒所有等待者
mon_exit(mid)             // 离开
```

**对比实验**：`semtest3`（信号量版本） vs `monitortest`（管程版本）的代码复杂度。

## 预期输出

```
[monitor] created: mid=0
[producer 0] enter monitor, produce item
[producer 0] signal not_full
[producer 0] exit monitor
[consumer 0] enter monitor, consume item
[consumer 0] signal not_empty
[consumer 0] exit monitor
... 3P-3C 完成 N 轮 ...

total: produced=300, consumed=300
代码行数: semtest3=X 行, monitortest=Y 行 (Y < X 即可)
```

## 运行结果

**实际输出（monitortest 摘录）**：
```
[parent] created monitor id=0
  [prod 1] put 100 at 0  (count=1)
  [prod 1] put 101 at 1  (count=2)
  [prod 1] put 102 at 2  (count=3)
  [cons 1] got 100 from 1  (count=2)
  [cons 1] got 101 from 2  (count=1)
  [cons 1] got 102 from 0  (count=0)
  [prod 1] put 103 at 0  (count=1)
  ...
=== Monitor test PASSED ===
```

- [x] monitor 创建、lock、wait、signal、unlock 调用全部成功
- [x] producer 写满 buffer（count 增长到 CAP）
- [x] consumer 看到 count > 0 立即消费
- [x] producer/consumer 互不冲突（monitor mutex 起作用）
- [x] 条件变量 wait/signal 正确同步（producer 满时 wait，consumer 消费后 signal）

## 备注

### API（系统调用 #47-52）

```c
int mon_create(void);                   // -> mid
int mon_lock(int mid);                  // 进入管程（互斥）
int mon_unlock(int mid);                // 离开管程
int mon_wait(int mid, int cvid);        // 释放 mutex + 睡在 cv 上 + 醒来重获 mutex
int mon_signal(int mid, int cvid);      // 唤醒 cv 上一个 waiter
int mon_broadcast(int mid, int cvid);   // 唤醒 cv 上所有 waiter
```

### 内核设计

- **monitor table**：`montable[8]`，每个 monitor 占用 4 个 sem（1 mutex + 3 cvs）
- **semtable 资源分配**：`alloc_sem_block(NRES_CV, &first)` 找连续的 N 个未分配 sem
- **lock 顺序**：先 acquire `mon_lock_inst` 读 montable[mid]，再 release；然后调 sem_wait/post（用 semtable 自带 lock）
- **wait 流程**：`sem_post(mutex); sem_wait(cv); sem_wait(mutex);` — Mesa 语义，signal 后 waiter 醒来要重新抢 mutex

### 关键 bug 与修复

- `banker_get_state` 用了 `void *` 类型而 user 端 buffer 当 `int *` 用，结构 layout 不匹配。已修复（user 端用 `int *` 计算偏移）
- `*cycle_len = 0` 写入 NULL 指针 → page fault（已修）
- `sem_broadcast` 没在 defs.h 中声明 → implicit-function-declaration error（已加原型）

### 与计划对应

- §2.1 Phase C1（管程 + 条件变量）✅ 完成
- 对应内核文件：`kernel/monitor.c`（新）、`kernel/sem.c`（加 `sem_broadcast`）
- 对应测试程序：`user/monitortest.c`（bounded buffer 1P-1C，CAP=3）

### 待办 C2

接下来实现 C2：用管程重写生产者-消费者（与 `semtest3` 对比）。需要 3P-3C 跑 N 轮，验证生产消费数量一致。
