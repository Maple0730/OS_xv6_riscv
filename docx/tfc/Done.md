## `wakeup()` 等待哈希队列优化完成内容

### 1. 完成目标

本次实现目标是把 `wakeup(chan)` 从原来的 **全进程表扫描** 改成 **按 `chan` 定位等待桶后局部遍历**，降低高频唤醒路径中的无效锁开销。

---

### 2.实际完成的代码修改

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
3. 修改 `procinit()`：
   - 初始化所有 `waittable[i].lock`
   - 初始化所有 `waittable[i].head`
   - 初始化每个 `proc` 的 `wnext/wprev/wbucket`
4. 修改 `freeproc()`：
   - 释放进程槽时同步清空等待链表元数据
5. 修改 `sleep()`：
   - 睡眠前根据 `chan` 定位等待桶
   - 将进程插入等待链表后再转入 `SLEEPING`
6. 修改 `wakeup()`：
   - 不再扫描整个 `proc[]`
   - 只遍历目标 `chan` 对应的等待桶链表
   - 命中等待者后从链表摘除并设为 `RUNNABLE`
7. 修改 `kkill()`：
   - 若目标进程正处于等待桶中，先从等待链表移除，再转为 `RUNNABLE`
   - 修正为与 `sleep()` / `wakeup()` 一致的锁顺序，避免死锁风险

---

### 3. 根因与修改逻辑链

#### 3.1 原始根因

原实现只有 `p->chan` 字段，没有建立 `chan -> 等待者集合` 的结构，因此 `wakeup(chan)` 无法直接找到等待进程，只能遍历整个 `proc[]`。

#### 3.2 修改逻辑链

```text
sleep(chan, lk)
  -> 根据 chan 计算 waitbucket
  -> 获取 waitbucket.lock 与 p->lock
  -> 将 p 插入对应等待链表
  -> 设置 p->chan 与 p->state = SLEEPING

wakeup(chan)
  -> 根据 chan 计算 waitbucket
  -> 获取 waitbucket.lock
  -> 只遍历该 bucket 的等待链表
  -> 找到 state == SLEEPING 且 p->chan == chan 的进程
  -> 从链表移除
  -> 设置 p->state = RUNNABLE
```

#### 3.3 一致性补充

除 `sleep()` / `wakeup()` 之外，本次还同步补齐了两类一致性维护：

- `freeproc()`：防止复用的 `proc` 槽位残留旧等待链信息
- `kkill()`：防止被杀死的睡眠进程继续残留在等待链表中

---

### 4. 代码影响范围

#### 实际修改文件

- `kernel/proc.h`
- `kernel/proc.c`

#### 未修改但直接受益的调用方

以下调用方保持原样，但其 `wakeup()` 路径会自动受益：

- `kernel/trap.c`
- `kernel/sysproc.c`
- `kernel/console.c`
- `kernel/pipe.c`
- `kernel/log.c`
- `kernel/uart.c`
- `kernel/virtio_disk.c`
- `kernel/sleeplock.c`
- `kernel/proc.c` 中的 `wait/exit/reparent` 相关唤醒点

这说明该优化仍然保持了“局部实现、全局受益”的设计目标。

---

### 5. 性能影响

#### 优化前

- `wakeup(chan)`：O(NPROC)
- 每次调用都扫描 64 个 `proc` 槽位
- 即使只有 1 个等待者，也会对大量无关进程执行锁操作

#### 优化后

- `wakeup(chan)`：近似 O(k)
- `k` 为目标等待桶中的进程数
- 大多数情况下只需访问少量真实等待者

#### 直接收益

1. 降低无关 `proc->lock` 的获取/释放次数
2. 降低多核下缓存行 bounce
3. 改善 `pipe`、`console`、`ticks`、`log`、`uart`、`virtio_disk` 等常见唤醒热点路径

#### 5.1 实验设计与性能对比说明

为验证本次优化的效果，建议采用“相同工作负载、优化前后对比”的方式进行实验。由于本次修改没有改变 `sleep()` / `wakeup()` 的对外接口，因此实验重点应放在 **唤醒成本** 与 **高频阻塞路径吞吐** 两个维度。

#### 实验环境建议

- 平台：QEMU RISC-V xv6
- CPU 数：`NCPU = 8`
- 进程上限：`NPROC = 64`
- 对比对象：
  - 基线版本：原始 `wakeup()` 全表扫描实现
  - 优化版本：当前 waitbucket 哈希等待队列实现

#### 建议工作负载

1. **时钟唤醒场景**  
   反复执行 `sleep(1)`、`sleep(2)` 之类依赖 `wakeup(&ticks)` 的用户程序，观察高频 ticks 唤醒路径。

2. **管道 ping-pong 场景**  
   使用两个或多个进程通过 `pipe` 反复读写，制造大量 `sleep()` / `wakeup()` 往返切换。

3. **控制台输入场景**  
   在 shell 中连续输入命令，观察 `console` 读路径上的唤醒响应。

4. **磁盘 / 日志路径场景**  
   执行文件创建、复制、批量写入等操作，使 `virtio_disk` 与 `log` 路径多次触发 wakeup。

#### 观测指标

1. **单次 `wakeup()` 扫描节点数**  
   - 优化前：固定接近 `NPROC`
   - 优化后：接近目标 bucket 内真实等待者数量

2. **单次唤醒涉及的锁操作次数**  
   - 优化前：近似 `2 * NPROC` 量级的 acquire/release 配对
   - 优化后：接近 `2 * k` 量级

3. **单位时间内完成的 ping-pong 轮数**  
   - 反映高频阻塞/唤醒场景下的吞吐差异

4. **平均 wakeup 延迟**  
   - 从事件发生到目标进程转为 `RUNNABLE` 的路径成本

5. **多核场景下的系统稳定性**  
   - 重点确认并发插链、摘链、kill 睡眠进程时无死锁或链表损坏

#### 预期结果

在等待者稀疏的典型场景下，优化版本应表现为：

- `wakeup()` 扫描范围显著缩小
- 无关 `proc->lock` 访问明显减少
- `pipe` / `ticks` / `console` / `disk` 等高频阻塞路径吞吐提升
- 多 CPU 下缓存争用下降，系统响应更稳定

如果等待者高度集中且哈希冲突严重，性能提升会变小；但在 xv6 常见的 channel 使用模式下，通常仍优于全表扫描。

#### 实验对比

可在实际测试完成后按如下形式记录结果：

```text
工作负载：pipe ping-pong，8 CPU，64 进程上限
对比维度：优化前 vs 优化后

1. 平均 wakeup 扫描节点数：64 -> 1~3
2. 单位时间 ping-pong 轮数：X -> Y
3. 平均 wakeup 路径耗时：A -> B
4. 结果结论：优化后在高频阻塞/唤醒路径上明显降低了无效扫描与锁竞争
```

---

### 6. 实现时做出的安全修正

实现过程中，相比初始草案补充了以下必要安全点：

1. **为每个 waitbucket 增加独立自旋锁**  
   否则多个 CPU 并发插链/摘链会破坏链表结构。

2. **`wakeup()` 遍历时先保存 `next` 再摘链**  
   否则删除当前节点后继续沿 `p->wnext` 遍历会失效。

3. **`kkill()` 调整为与主路径一致的锁顺序**  
   统一采用 `waitbucket.lock -> p->lock`，避免与 `sleep()` / `wakeup()` 形成 AB/BA 死锁。

因此，最终落地版本不仅实现了性能优化，也补齐了真实并发环境下所需的正确性约束。

---

### 7. 结论

本次已完成一个低耦合、较独立的调度/阻塞子系统优化：

- 不改外部 API
- 仅修改 `proc` 核心实现
- 让 `wakeup()` 从全表扫描转为按 `chan` 局部查找
- 同时补齐了 `kkill()` 与 `freeproc()` 的等待链一致性维护