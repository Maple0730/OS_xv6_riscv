## 等待哈希队列优化方案设计

### 总体思路

在 `kernel/proc.c` / `kernel/proc.h` 中增加一个仅服务于睡眠-唤醒路径的 **wait-channel 哈希等待队列**：

- `sleep(chan, lk)`：在进程真正进入 `SLEEPING` 前，将其插入 `chan` 对应 bucket 的链表。
- `wakeup(chan)`：通过哈希定位 bucket，仅扫描该 bucket 中的等待者。
- 被唤醒的进程在状态切换为 `RUNNABLE` 时，从等待链表中摘除。

### 需要新增的数据结构

建议在 `kernel/proc.h` 中新增：

```c
#define NWCHAN 64

struct waitbucket {
  struct spinlock lock;
  struct proc *head;
};
```

并在 `kernel/proc.c` 中定义：

```c
struct waitbucket waittable[NWCHAN];
```

同时在 `struct proc` 中增加链表指针：

```c
struct proc *wnext;
struct proc *wprev;
```

以及可选的辅助字段：

```c
struct waitbucket *wbucket;
```

### 为什么要给 bucket 单独加锁

这是本方案中最重要的实现约束。

如果只有 `p->lock` 而没有 `waitbucket.lock`，会出现两个问题：

1. 多个 CPU 可同时对同一 bucket 链表插入/删除，链表结构无法安全维护。
2. `wakeup()` 遍历链表时，节点可能被并发摘除，导致遍历指针失效。

因此，**哈希表本身必须受 bucket 粒度锁保护**。这也是该优化方案要安全落地所必须补齐的同步机制。

### 推荐锁顺序

为避免死锁，推荐统一锁顺序：

- 先拿 `waitbucket.lock`
- 再拿 `p->lock`

并在所有涉及等待链表的路径中保持一致。

推荐理由：

- `sleep()` 本来就会在进入睡眠前拿 `p->lock`，这里需调整为在链表插入阶段遵守统一顺序。
- `wakeup()` 需要先稳定住 bucket 链表，再逐个检查进程状态。
- 统一顺序后可以避免 `sleep()` 与 `wakeup()` 在不同 CPU 上形成 AB/BA 死锁。

---

### 具体改动位置与改动逻辑

### `kernel/proc.h`

#### 修改位置

- `kernel/proc.h:79` 附近：在 `enum procstate` 后新增等待队列结构定义。
- `kernel/proc.h:82` 附近：在 `struct proc` 中新增等待链表指针字段。

#### 计划改动

1. 新增 `NWCHAN` 常量。
2. 新增 `struct waitbucket`。
3. 在 `struct proc` 中新增：
   - `struct proc *wnext;`
   - `struct proc *wprev;`
   - 可选：`struct waitbucket *wbucket;`

#### 修改逻辑链

- 现有 `p->chan` 只能表示“我在等谁”。
- 新增链表指针后，进程还能表示“我在某个等待队列中的前驱/后继”。
- 新增 bucket 结构后，内核可以表达“某个 `chan` 的等待进程集合”。

#### 影响评估

- 只影响 `proc` 子系统内部数据结构。
- 不改变任何对外函数签名。
- 不要求其他模块修改其 `sleep()` / `wakeup()` 调用方式。

---

### 6.2 `kernel/proc.c` 全局区与 `procinit()`

#### 修改位置

- `kernel/proc.c:9-16` 附近：全局变量区域。
- `kernel/proc.c:47-59` 的 `procinit()`。

#### 计划改动

1. 在全局区增加：
   - `struct waitbucket waittable[NWCHAN];`
2. 在 `procinit()` 中初始化每个 bucket：
   - `initlock(&waittable[i].lock, "waitbucket");`
   - `waittable[i].head = 0;`
3. 顺便初始化每个 `proc` 的等待链指针：
   - `p->wnext = 0;`
   - `p->wprev = 0;`
   - `p->wbucket = 0;`

#### 修改逻辑链

- `procinit()` 目前只初始化进程锁与进程槽状态。
- 引入等待队列后，必须在系统启动时同时初始化等待哈希表。
- 这样后续 `sleep()` / `wakeup()` 可直接访问 bucket，而不需要额外懒初始化路径。

#### 影响评估

- 仅增加少量静态初始化开销。
- 不影响 `userinit()`、`scheduler()`、`fork()` 等行为。

---

### `kernel/proc.c:543` 的 `sleep()`

#### 修改位置

`sleep()` 位于 `kernel/proc.c:543`。

#### 优化前关键逻辑

```543:569:kernel/proc.c
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  release(lk);

  p->chan = chan;
  p->state = SLEEPING;

  sched();

  p->chan = 0;
  release(&p->lock);
  acquire(lk);
}
```

#### 优化后逻辑

建议改为以下时序：

1. 计算 `chan` 对应 bucket。
2. 先拿 `waitbucket.lock`。
3. 再拿 `p->lock`。
4. 释放原条件锁 `lk`。
5. 设置 `p->chan = chan`。
6. 把 `p` 挂入 bucket 链表。
7. 设置 `p->state = SLEEPING`。
8. 释放 `waitbucket.lock`。
9. 调用 `sched()`。
10. 返回后清理 `p->chan`、`wnext`、`wprev`、`wbucket` 的残余状态。
11. 释放 `p->lock`，重新获取 `lk`。

#### 为什么这里不能只按原计划简单插链

如果照附件计划中“只靠 `p->lock` 插链”的思路实现，会存在两个具体风险：

- **风险 1：链表并发损坏**  
  多个 CPU 同时执行 `sleep()` 向同一 bucket 插入节点时，没有 bucket 锁保护会直接破坏 `next/prev` 指针。

- **风险 2：`wakeup()` 遍历失效**  
  `wakeup()` 在摘除当前节点后若直接执行 `p = p->wnext`，而 `p->wnext` 已被清零，就会提前终止或丢失后继节点。

因此，实际文档中必须把 bucket 锁与“先保存 next 再摘链”的细节写清楚。

#### 影响评估

- `sleep()` 仍保持原签名：`sleep(void *chan, struct spinlock *lk)`。
- 所有调用方代码无需改动。
- 插链新增的是 O(1) 常数开销，用一次 bucket 锁换掉后续大量无效全表扫描，收益更高。

---

### 6.4 `kernel/proc.c:574` 的 `wakeup()`

#### 修改位置

`wakeup()` 位于 `kernel/proc.c:574`。

#### 优化后核心逻辑

1. 通过 `chan` 计算 bucket 下标。
2. 获取 `waitbucket.lock`，稳定住该 bucket 链表。
3. 从 `bucket->head` 开始遍历。
4. 对每个节点：
   - 先保存 `next = p->wnext`
   - 获取 `p->lock`
   - 检查 `p->state == SLEEPING && p->chan == chan`
   - 若匹配，则：
     - 从 bucket 链表摘除
     - 将 `p->state = RUNNABLE`
     - 清理 `wnext/wprev/wbucket`
   - 释放 `p->lock`
   - 继续访问保存好的 `next`
5. 释放 `waitbucket.lock`

#### 推荐伪代码

```c
void
wakeup(void *chan)
{
  struct waitbucket *wb = &waittable[hash_chan(chan)];
  struct proc *p;
  struct proc *next;

  acquire(&wb->lock);
  for (p = wb->head; p != 0; p = next) {
    next = p->wnext;
    acquire(&p->lock);
    if (p->state == SLEEPING && p->chan == chan) {
      waitlist_remove(wb, p);
      p->state = RUNNABLE;
      p->wprev = 0;
      p->wnext = 0;
      p->wbucket = 0;
    }
    release(&p->lock);
  }
  release(&wb->lock);
}
```

#### 修改逻辑链

- 原逻辑的主要成本来自“找人”。
- 哈希等待队列后，`wakeup()` 不再从系统所有进程中找目标，而是直接从“可能等待这个 `chan` 的集合”中找目标。
- 扫描范围从 `proc[NPROC]` 收缩为对应 bucket 链表。

### 影响评估

- 调用语义不变，仍然是“唤醒所有在 `chan` 上睡眠的进程”。
- 对 `console`、`pipe`、`ticks`、`virtio_disk`、`log` 等现有等待点透明。
- 代码复杂度主要增加在 `proc.c` 内部，外部模块零改动。

---

### `kernel/proc.c` 的 `freeproc()` / `kkill()` 辅助收尾

#### 修改位置

- `kernel/proc.c:156` 的 `freeproc()`
- `kernel/proc.c:592` 的 `kkill()`

#### 建议补充改动

##### `freeproc()`

在现有清理逻辑中增加：

- `p->wnext = 0;`
- `p->wprev = 0;`
- `p->wbucket = 0;`

这样保证 `UNUSED` 进程槽不会带着旧等待链表元数据复用。

##### `kkill()`

当前 `kkill()` 在进程处于 `SLEEPING` 时会直接执行：

```597:609:kernel/proc.c
for (p = proc; p < &proc[NPROC]; p++) {
  acquire(&p->lock);
  if (p->pid == pid) {
    p->killed = 1;
    if (p->state == SLEEPING) {
      p->state = RUNNABLE;
    }
    release(&p->lock);
    return 0;
  }
  release(&p->lock);
}
```

引入等待队列后，若被 kill 的目标仍挂在 wait bucket 中，则还需要把它从等待链表摘除，否则 bucket 会保留失效节点。

#### 修改逻辑链

- 现有系统允许 `kkill()` 直接把睡眠进程改成 `RUNNABLE`。
- 新方案中，`SLEEPING` 不再只是状态，还伴随一个等待链表成员身份。
- 所以 `kkill()` 必须同时完成“改状态 + 脱链”。

#### 影响评估

- 不改变 `kill` 的对外语义。
- 仅补齐等待队列与进程状态之间的一致性维护。

---

### 相关涉及代码位置

以下位置与本次设计直接相关：

#### 核心修改文件

- `kernel/proc.h:79`
- `kernel/proc.h:82`
- `kernel/proc.c:47`
- `kernel/proc.c:156`
- `kernel/proc.c:543`
- `kernel/proc.c:574`
- `kernel/proc.c:592`

#### 受益但无需改动的调用方

- `kernel/trap.c:172`
- `kernel/sysproc.c:83`
- `kernel/console.c:104`
- `kernel/console.c:180`
- `kernel/pipe.c:64`
- `kernel/pipe.c:67`
- `kernel/pipe.c:90`
- `kernel/pipe.c:99`
- `kernel/pipe.c:118`
- `kernel/pipe.c:131`
- `kernel/log.c:133`
- `kernel/log.c:136`
- `kernel/log.c:163`
- `kernel/log.c:173`
- `kernel/virtio_disk.c:180`
- `kernel/virtio_disk.c:231`
- `kernel/virtio_disk.c:284`
- `kernel/virtio_disk.c:320`
- `kernel/uart.c:89`
- `kernel/uart.c:149`
- `kernel/sleeplock.c:26`
- `kernel/sleeplock.c:39`
- `kernel/proc.c:318`
- `kernel/proc.c:354`
- `kernel/proc.c:413`

---

### 根因、修改逻辑链与性能影响总结

### 根因

- 进程表 `proc[]` 是扁平数组。
- `struct proc` 只有 `p->chan`，没有等待队列索引。
- `wakeup(chan)` 无法通过 `chan` 直接找到等待者，只能全表扫描。

### 修改逻辑链

```text
原始状态
  -> sleep() 仅写入 p->chan
  -> wakeup() 无索引
  -> 只能扫描 proc[] 全表
  -> 为大量无关进程重复加锁

优化后
  -> 新增 waitbucket 哈希表
  -> sleep() 将进程挂入 chan 对应 bucket
  -> wakeup() 先按 chan 定位 bucket
  -> 只遍历 bucket 中可能相关的等待者
  -> 对命中的等待者直接摘链并转 RUNNABLE
```

### 性能影响

#### 时间复杂度变化

- 优化前：`wakeup()` 为 O(NPROC)
- 优化后：`wakeup()` 为 O(k + c)
  - `k`：目标 bucket 中的等待进程数
  - `c`：固定哈希与 bucket 锁开销

#### 典型收益

1. **减少无效锁操作**  
   不再对大量无关 `proc` 执行 `acquire/release`。

2. **降低跨核缓存一致性流量**  
   原实现会依次触碰很多 `proc.lock`；新实现只触碰目标 bucket 中的少量等待者。

3. **提升 I/O 密集型路径效率**  
   `pipe`、`console`、`uart`、`virtio_disk`、`log`、`ticks` 等频繁唤醒场景都会受益。

4. **保持模块独立性**  
   优化集中在 `proc` 子系统内部，不改变其他模块 API。

#### 性能边界

- 最坏情况下，若大量不同 `chan` 哈希冲突到同一 bucket，复杂度会退化，但仍通常优于扫描全部 `proc[]`。
- 当系统等待者很少时，收益主要表现为减少锁与缓存开销，而不是改变功能语义。

---

### 风险与兼容性

#### 兼容性

- `sleep()` / `wakeup()` 签名不变。
- 调用方不需要改动。
- 进程状态语义不变：仍然使用 `SLEEPING` / `RUNNABLE`。

#### 主要风险

1. **链表并发一致性**  
   若 bucket 无独立锁，方案不可安全实现。

2. **kill 路径遗漏脱链**  
   若 `kkill()` 只改状态不摘链，会残留失效等待节点。

3. **遍历时摘链顺序错误**  
   若 `wakeup()` 未先保存 `next` 再摘链，可能跳过后继或访问失效指针。

4. **锁顺序不统一**  
   若部分路径先拿 `p->lock`、部分路径先拿 bucket 锁，可能产生死锁。

---