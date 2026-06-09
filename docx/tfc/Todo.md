## 进程管理和处理器调度

### `wakeup()` 哈希等待队列优化修改

`wakeup()` 当前实现位于 `kernel/proc.c:574`：
对应的睡眠路径位于 `kernel/proc.c:543`：
进程睡眠状态字段位于 `kernel/proc.h:82`：

### 修改理由

当前 xv6 只在 `struct proc` 中保存单个 `p->chan`，但没有建立 “`chan` -> 等待进程集合” 的索引结构
因此完整逻辑链为：
1. 某个进程在 `sleep(chan, lk)` 中将 `p->chan = chan` 并进入 `SLEEPING`。
2. 内核没有把这个进程挂入某个按 `chan` 组织的等待队列。
3. 事件发生后，`wakeup(chan)` 无法直接定位等待者。
4. 结果只能扫描 `proc[NPROC]` 全表，并逐个尝试 `acquire(&p->lock)`。
5. 在多数场景下，真正匹配 `chan` 的进程数很少，但仍要为所有槽位支付锁操作和缓存一致性成本。

---

### 现有性能问题

#### 复杂度问题

当前 `wakeup()` 每次调用复杂度为 O(NPROC)。
```1:3:kernel/param.h
#define NPROC       64
```
这意味着一次 `wakeup(chan)` 在最坏情况下要扫描 64 个进程槽位。

#### 修改函数调用位置

`wakeup()` 被多个常见阻塞点调用，例如：

- `kernel/trap.c:172` 的 `wakeup(&ticks)`
- `kernel/console.c:180` 的 `wakeup(&cons.r)`
- `kernel/pipe.c:64`、`kernel/pipe.c:67`、`kernel/pipe.c:99`、`kernel/pipe.c:131`
- `kernel/log.c:163`、`kernel/log.c:173`
- `kernel/virtio_disk.c:180`、`kernel/virtio_disk.c:320`
- `kernel/sleeplock.c:39`
- `kernel/proc.c:318`、`kernel/proc.c:354`

这些调用点覆盖时钟、控制台输入、管道、日志、磁盘 I/O、sleep lock 与父子进程唤醒路径，因此 `wakeup()` 的线性扫描是一个高频通用开销。

---

### 优化目标

将 `wakeup(chan)` 的主要查找路径从：
- **优化前**：扫描整个 `proc[]`，复杂度 O(NPROC)
改为：
- **优化后**：先通过 `chan` 哈希定位 bucket，再仅遍历该 bucket 内等待链表，复杂度近似 O(k)
其中 `k` 为落在同一 `chan` 上的等待进程数。典型情况下，`k` 远小于 `NPROC`。

---

### 方案思路

在 `kernel/proc.c` / `kernel/proc.h` 中增加一个仅服务于睡眠-唤醒路径的 **wait-channel 哈希等待队列**：

- `sleep(chan, lk)`：在进程真正进入 `SLEEPING` 前，将其插入 `chan` 对应 bucket 的链表。
- `wakeup(chan)`：通过哈希定位 bucket，仅扫描该 bucket 中的等待者。
- 被唤醒的进程在状态切换为 `RUNNABLE` 时，从等待链表中摘除。

---