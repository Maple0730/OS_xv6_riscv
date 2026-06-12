# Conversation Handoff

这个文件用于在新会话中快速恢复当前项目上下文。当前主线已经不是早期的构建系统调整，而是 `xv6-riscv` 中 `kmalloc/kmfree` 的设计、实现、验证和文档整理。

## 1. 当前阶段

当前项目的工作重点是：

- 实现内核堆分配器 `kmalloc/kmfree`
- 在 `kalloc/kfree` 之上补一层小对象动态分配能力
- 验证它能否进入真实内核对象路径
- 将技术路线、实现结果、测试与分析写入 `docx/Maple/Doing.md`

当前结论：

- `kmalloc/kmfree` 这一版已经可以算**阶段性完成**
- 它已经不是“只有原型”的状态，而是已有真实接入点和自测结果的可运行实现

## 2. 当前仓库状态

当前工作区不是干净的，切换会话时不要误回滚：

- `kernel/kmalloc.c`：已实现新版 `kmalloc/kmfree`
- `kernel/file.c`：已将 `struct file` 改为动态分配
- `docx/Maple/Doing.md`：已补充大量技术方案、测试结果和分析
- `build/kernel/kernel`：构建产物变更

最近已验证：

- `make`
- `timeout 15s make qemu`
- 交互式 `make qemu` 下的多条 shell 命令

## 3. kmalloc/kmfree 当前实现

实现文件：

- [kernel/kmalloc.c](/home/hanshuo/office/OS/xv6-riscv/kernel/kmalloc.c)
- [kernel/defs.h](/home/hanshuo/office/OS/xv6-riscv/kernel/defs.h)
- [kernel/main.c](/home/hanshuo/office/OS/xv6-riscv/kernel/main.c)
- [Makefile](/home/hanshuo/office/OS/xv6-riscv/Makefile)

### 已实现能力

1. 固定 size class 小对象分配

- `32`
- `64`
- `128`
- `256`
- `512`
- `1024`
- `2048`

2. 基本行为

- `kmalloc(0)` 返回 `0`
- 小对象按“向上取整到最近档位”分配
- `kmfree(0)` 安全返回
- 释放后对象可复用

3. slab 扩容

- 某个档位没有空闲对象时
- 自动通过 `kalloc()` 申请新页
- 切分成该档位对象

4. 单页大对象回退

- `0 < n <= 2048`：走 slab
- `2048 < n <= big_object_capacity()`：走单页大对象回退
- `n > big_object_capacity()`：返回 `0`

注意：

- 当前**不支持连续多页大对象分配**
- 用户之前已经接受这一边界

5. 空 slab 页归还

- 每个 class 维护的是“页链表”
- 每页维护自己的 freelist
- 当某页 `free_objs == total_objs` 时
- 会从 class 链表摘除并交回 `kfree()`

6. 统计与调试输出

当前 `kmallocdump()` 会输出：

- 每个 class 的 `pages/total/free/used`
- `alloc_ok`
- `alloc_fail`
- `free_ok`
- `big_alloc_ok`
- `big_free_ok`
- `big_active_pages`
- `slab_pages_returned`

### 关键设计边界

- 非法 `kmfree()` 仍然 `panic`
- 大于单页对象区的 `kmalloc()` 不 `panic`，返回 `0`
- 还没有双重释放检测
- 还没有连续多页分配器
- 还没有通用 malloc 风格自由链表

## 4. 启动接入与自测

接入位置：

- [kernel/main.c](/home/hanshuo/office/OS/xv6-riscv/kernel/main.c:19)

当前启动顺序中：

1. `kinit()`
2. `kheapinit()`
3. `kmalloctest()`
4. `kvminit()`

也就是说，每次启动内核都会自动跑一轮 `kmalloc` 自测。

### 当前自测覆盖内容

- `kmalloc(0)` 返回 `0`
- `kmalloc(bigmax + 1)` 返回失败
- `kmalloc(1)`
- `kmalloc(32)`
- `kmalloc(33)`
- `kmalloc(2000)`
- `kmalloc(2049)`
- `kmalloc(3000)`
- `kmalloc(bigmax)`
- 可写性测试
- 释放后地址复用测试
- 批量 `32B` 分配触发跨页扩容
- 释放后 slab 页归还
- 大对象页归还

### 当前典型启动输出

```text
xv6 kernel is booting

kmalloctest: begin
kmalloctest: boundary checks ok
kmalloctest: basic allocations ok
kmalloctest: read/write checks ok
kmalloctest: reuse check ok
kmalloctest: cross-page growth ok (140 objects)
kmalloc stats:
  class  size  pages  total  free  used
  0 32 0 0 0 0
  1 64 0 0 0 0
  2 128 0 0 0 0
  3 256 0 0 0 0
  4 512 0 0 0 0
  5 1024 0 0 0 0
  6 2048 0 0 0 0
  alloc_ok=148 alloc_fail=1 free_ok=148
  big_alloc_ok=3 big_free_ok=3 big_active_pages=0
  slab_pages_returned=4
  slab_pages=0 slab_objs=0 free_objs=0 used_objs=0
kmalloctest: ok
```

从这组结果可知：

- 单页大对象回退路径已生效
- 空 slab 页已真正归还到底层页分配器
- 本轮测试结束后没有残留活动对象页

## 5. 已完成的真实接入点

### 接入点 1：`struct pipe`

文件：

- [kernel/pipe.c](/home/hanshuo/office/OS/xv6-riscv/kernel/pipe.c)

当前状态：

- `pipealloc()` 已从 `kalloc()` 改为 `kmalloc(sizeof(*pi))`
- `pipeclose()` 的最终释放已改为 `kmfree(pi)`
- `pipe` 是“真实小对象误用整页分配”的典型修复案例

已验证命令：

```text
echo hello | wc
cat README | wc
echo alpha | grep alpha
```

对应结果：

```text
1 1 6
128 220 3318
alpha
```

### 接入点 2：`struct file`

文件：

- [kernel/file.c](/home/hanshuo/office/OS/xv6-riscv/kernel/file.c)

注意这个案例的性质和 `pipe` 不同：

- `struct file` 原来**不是** `kalloc()` 分配
- 它原来是静态 `file[NFILE]` 对象池
- 这次改造是“静态 file 表动态化”
- 不是“整页误用修复”

当前实现：

- `ftable` 从静态 `file[NFILE]` 改为只维护 `nfile`
- `filealloc()` 在 `nfile < NFILE` 时调用 `kmalloc(sizeof(*f))`
- `fileclose()` 在引用计数归零后最终 `kmfree(f)`
- 保留了 `NFILE` 上限约束

已验证命令：

```text
echo hello
cat README | wc
echo alpha | grep alpha
echo beta > kmfile
cat kmfile
rm kmfile
cat README | grep xv6 | wc
```

对应结果：

```text
hello
128 220 3318
alpha
beta
13 51 1072
```

说明：

- 文件读写路径正常
- 管道路径正常
- 重定向、删除正常
- `struct file` 动态分配/回收没有打坏主路径

## 6. 文档状态

核心文档：

- [docx/Maple/Doing.md](/home/hanshuo/office/OS/xv6-riscv/docx/Maple/Doing.md)

当前 `Doing.md` 已经写到以下内容：

1. `kmalloc/kfree` 首版技术路线
2. 启动自测使用教程、结果与分析
3. 第一个真实接入点 `struct pipe`
4. 第二个真实接入点 `struct file`
5. 大对象自动退回整页分配：
   - 技术路线
   - 当前实现
   - 测试结果
   - 结果分析
6. 空 slab 页归还给 `kfree()`：
   - 技术路线
   - 当前实现
   - 测试结果
   - 结果分析

另外要注意：

- “Maple” 是用户个人文档命名空间
- 除了交给老师的正式报告，其它内部技术记录都写到 `docx/Maple/`

## 7. 当前判断

如果新会话需要判断“这版 `kmalloc/kmfree` 算不算完成”，当前结论应当是：

- 作为课程项目当前阶段：**算完成**
- 作为长期可扩展内核堆：**还有优化空间**

更准确的表达：

- 这一版已经有完整小对象分配/释放能力
- 已支持单页大对象回退
- 已支持空 slab 页归还
- 已接入真实对象并验证
- 因此可以算“第一版完整实现”

## 8. 尚未做但可继续做的事情

这些现在都属于增强项，不是这版必须项：

1. 连续多页大对象分配

- 现在超过单页对象区的申请会失败
- 如果要支持更大对象，需要先设计连续多页分配器

2. 更强的非法释放检测

- 例如双重释放检测
- 当前主要依赖页头和基本状态校验

3. 更多真实对象接入

- 但要先分清：
  - 是“整页误用修复”
  - 还是“静态对象池动态化”

4. 更系统的压力测试

- 当前已有功能测试
- 还可以继续补更激进的混合分配/释放压力测试

5. 课程报告整理

- `Doing.md` 已经像“内部技术记录”
- 后续若要交老师，应该再压缩成更正式的实验报告版本

## 9. 新会话建议切入点

建议新会话直接这样开始：

```text
请先阅读 HANDOFF.md 和 docx/Maple/Doing.md，再基于当前代码继续帮助我。

当前重点是 xv6-riscv 中的 kmalloc/kmfree。现在这版已经支持：
- size-class 小对象分配
- 单页大对象自动回退
- 空 slab 页归还给 kfree()
- pipe 和 file 两个真实接入点

请基于当前实现继续，不要回到早期的构建系统迁移话题。
如果需要改代码，直接在仓库里实现并验证。
```

## 10. 额外提醒

1. 用户主要使用中文交流。

2. 用户名字是“韩硕”，但在个人技术文档语境下更喜欢用 “Maple”。

3. 用户多次强调：

- 除了要交给老师的正式报告
- 其它技术方案、实现记录、测试记录都写到 `docx/Maple/`

4. 用户当前最关心的问题不是“概念解释到无限细”，而是：

- 当前做到了什么
- 还没做到什么
- 这一版算不算完成
- 下一步最值得继续做什么
