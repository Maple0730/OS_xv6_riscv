# 内存管理

## 一、虚拟内存管理：页表创建、映射、解除映射

对应核心文件：`kernel/vm.c`

### 1. 页表创建

已实现以下函数：
- `kvmmake()`：创建内核页表
- `uvmcreate()`：创建用户页表
- `proc_pagetable()`：为特定进程建立用户页表骨架

实现原理：
- 调用 `kalloc()` 分配一页作为页表根页
- 清零该页
- 后续通过 `walk()` 逐级建立页表项（PTE）

### 2. 页表映射

已实现以下函数：
- `walk()`：按 Sv39 三级页表结构，找到虚拟地址对应的最低级 PTE
- `mappages()`：建立虚拟地址到物理地址的映射
- `kvmmap()`：为内核页表添加映射

实际映射示例：
- 内核页表（`kvmmake`）中映射了 UART、VIRTIO、PLIC、内核代码段、内核数据段、TRAMPOLINE
- 用户空间扩展（`uvmalloc`）为新页分配物理内存并建立映射
- 进程页表（`proc_pagetable`）中映射 TRAMPOLINE 和 TRAPFRAME

### 3. 解除映射

已实现以下函数：
- `uvmunmap()`：解除用户页的映射
- `uvmdealloc()`：收缩用户空间时解除一段映射
- `uvmfree()`：释放整个用户地址空间
- `proc_freepagetable()`：释放进程页表，清理固定映射和用户页

核心函数 `uvmunmap()` 将对应 PTE 清零，并在需要时释放背后的物理页。

### 4. 配套能力

- `walkaddr()`：将用户虚拟地址翻译为物理地址
- `uvmcopy()`：`fork` 时复制父进程的用户地址空间
- `uvmalloc()` / `uvmdealloc()`：用户空间扩展与收缩
- `uvmfree()` / `freewalk()`：完整回收页表及用户页
- `vmfault()`：缺页处理，支持懒分配

### 小结

> **当前系统已实现基于 Sv39 的虚拟内存管理机制**，包括内核/用户页表创建、虚拟地址到物理地址映射、地址解除映射、用户地址空间扩展与回收，以及缺页触发下的延迟分配支持。

---

## 二、用户态内存空间布局（代码段、数据段、堆、栈）

对应主要文件：  
`kernel/exec.c`, `kernel/proc.c`, `kernel/vm.c`, `kernel/memlayout.h`

### 1. 整体布局

一个用户进程的地址空间从低地址到高地址依次为：
- 代码段（text）
- 数据段 / bss
- 堆（heap）
- 用户栈（stack）
- 最高地址附近固定映射：`TRAPFRAME` 和 `TRAMPOLINE`

### 2. 代码段、数据段（含 bss）的来源

由 `kernel/exec.c` 中的 `exec()` 实现：
- 读取 ELF 文件后，按程序头（program header）将各段装入用户页表
- 可执行段 → 代码段
- 可读写段 → 数据段
- `p_memsz > p_filesz` 的部分自然形成 bss（未初始化数据区）

### 3. 堆的实现

- `exec()` 装载完成后，进程当前大小 `p->sz` 指向堆的起始位置
- 通过 `sbrk()` 或 `sbrklazy()` 系统调用扩展堆区
- 原理：`sys_sbrk()` → `growproc()`（或懒分配）扩展 `p->sz`

### 4. 用户栈的实现

- 在 `exec()` 中额外分配用户栈空间，并压入命令行参数
- 定义 `USERSTACK` 指定栈地址
- 使用 `uvmclear()` 设置栈页下方的 guard page（保护页）
- 栈参数传递符合 RISC-V 调用约定

### 5. 高地址特殊区域

- `TRAPFRAME`：保存用户态寄存器，用于 trap 恢复
- `TRAMPOLINE`：用户↔内核态切换的跳板代码

这些固定页属于用户地址空间顶部布局的一部分，支撑 trap 进出内核。

### 6. 现状总结

- **基础用户空间布局已完整实现**：代码段、数据段、堆、用户栈
- 支持 `exec` 装载和 `sbrk` 扩展
- 支持 trap 相关高地址固定页映射

**注意简化点**（与复杂操作系统如 Linux 的区别）：
- 没有复杂的 VMA（虚拟内存区域）管理
- 未实现 `mmap` 系统调用
- 栈布局较为简单
- 段权限模型偏教学化

### 7. 报告建议表述

> 当前系统已经实现基础用户态内存空间布局，包括代码段、数据段、堆和用户栈。代码段和数据段由 ELF 装载器在 `exec` 过程中建立，堆通过 `sbrk` 接口动态扩展，用户栈由 `exec` 初始化并完成参数压栈；同时在高地址固定映射 `TRAPFRAME` 和 `TRAMPOLINE`，以支撑用户态与内核态切换。

**一句话结论**：  
**用户态内存空间布局（代码段、数据段、堆、栈）已经实现**，不是仅仅有页表，而是完整的布局及配套管理机制。

---

## 三、内核堆内存分配（`kmalloc`/`kmfree`）

本项目在 xv6 原有页级分配器（`kalloc`/`kfree`）之上，实现了一层完整的内核堆分配器（`kmalloc`/`kmfree`），支持小对象 slab 管理、单页大对象回退、空 slab 页自动归还，并成功将 `struct pipe` 和 `struct file` 两个真实内核对象接入该分配器。

当前内存管理形成清晰的两层架构：
- **页级分配器**：`kalloc` / `kfree`，管理 4KB 物理页
- **堆级分配器**：`kmalloc` / `kmfree`，管理内核中小块动态对象

### 1. 设计思路

- 采用 **size‑class slab 分配器**，按固定大小档位管理小对象
- 每个档位从 `kalloc()` 申请一页，切分为等大小对象
- 对象释放后回到对应档位的空闲链表，实现复用
- 对象地址按指针大小对齐，支持零值安全释放

### 2. Size Class 档位

| Class | 对象大小（字节） |
|-------|------------------|
| 0     | 32               |
| 1     | 64               |
| 2     | 128              |
| 3     | 256              |
| 4     | 512              |
| 5     | 1024             |
| 6     | 2048             |

### 3. 接口

```c
void  kheapinit(void);
void *kmalloc(uint n);
void  kmfree(void *p);
```

- `kmalloc(0)` 返回 `0`
- `0 < n <= 2048` 走对应 size class
- 更大对象由“单页大对象回退”处理（见第 4 节）

### 4. 自测验证

在 `main.c` 启动流程中自动执行 `kmalloctest()`，覆盖：
- 边界值检查（0、2049）
- 基本分配（1、32、33、2000）
- 读写测试
- 复用测试
- 跨页扩展（连续申请 140 个 32B 对象）

**典型输出（首版）**：
```
kmalloctest: begin
kmalloctest: boundary checks ok
kmalloctest: basic allocations ok
kmalloctest: read/write checks ok
kmalloctest: reuse check ok
kmalloctest: cross-page growth ok (140 objects)
kmalloc stats:
  class  size  pages  total  free  used
  0     32    2      254    254   0
  1     64    1      63     63    0
  2     128   0      0      0     0
  3     256   0      0      0     0
  4     512   0      0      0     0
  5     1024  0      0      0     0
  6     2048  1      1      1     0
  alloc_ok=145 alloc_fail=1 free_ok=145
  total_pages=4 total_objs=318 free_objs=318 used_objs=0
kmalloctest: ok
```

### 5. 真实内核对象接入

#### 5.1 `struct pipe` —— 纠正整页浪费

**原始问题**：`struct pipe` 远小于 4KB，却使用 `kalloc()` 独占一整页。

**改动**：
- `pipealloc()`：`kalloc()` → `kmalloc(sizeof(*pi))`，并 `memset` 清零
- `pipeclose()`：`kfree()` → `kmfree(pi)`

**测试**（真实 shell 命令）：
```bash
echo hello | wc        # 输出 1 1 6
cat README | wc        # 输出 128 220 3318
echo alpha | grep alpha # 输出 alpha
```
所有管道命令正常执行，无 panic。

#### 5.2 `struct file` —— 静态表动态化

**原始特点**：全局静态 `file[NFILE]` 数组，线性扫描复用。

**改动**：
- 移除静态数组，改为维护 `nfile` 计数与 `NFILE` 上限
- `filealloc()`：检查 `nfile < NFILE`，调用 `kmalloc(sizeof(*f))` 动态分配
- `fileclose()`：引用计数归零后调用 `kmfree(f)` 释放

**测试**：
```bash
echo hello
cat README | wc
echo alpha | grep alpha
echo beta > kmfile && cat kmfile && rm kmfile
cat README | grep xv6 | wc
```
所有文件读写、重定向、管道组合均正常，无 panic。

### 6. 单页大对象自动回退

#### 6.1 问题与目标

- 原实现中 `n > 2048` 直接失败
- 新目标：`2048 < n <= 单页可用对象区` 时，回退到整页分配，对象独占一页

#### 6.2 实现要点

- 引入统一页头 `kmem_page`，标记页类型：
  - `KMEM_PAGE_SLAB`（小对象页）
  - `KMEM_PAGE_BIG`（大对象页）
- `kmalloc` 分流：
  - `0 < n <= 2048` → slab 路径
  - `2048 < n <= PGSIZE - sizeof(kmem_page)` → 调用 `kalloc()`，初始化页头，返回页头后地址
  - 更大 → 失败
- `kmfree` 根据页头类型：
  - `KMEM_PAGE_SLAB` → 走 slab 释放逻辑
  - `KMEM_PAGE_BIG` → 检查地址合法性，直接 `kfree()` 整页

#### 6.3 测试结果

```text
kmalloctest: begin
...
alloc_ok=148 alloc_fail=1 free_ok=148
big_alloc_ok=3 big_free_ok=3 big_active_pages=0
...
kmalloctest: ok
```

- `big_alloc_ok=3`：成功分配 `2049`、`3000` 及边界最大值
- `big_free_ok=3`：三个大对象均正确回收
- `big_active_pages=0`：无残留大对象页

### 7. 空闲 slab 页自动归还 `kfree()`

#### 7.1 问题与改进

- 早期 slab 实现只有对象级复用，页永远不归还
- 改进：将组织方式从“class 全局 freelist”改为 **“class 页链表 + 页内 freelist”**

#### 7.2 关键数据结构

- **每个 size class**：
  - 页链表（带头节点）
  - 锁、页数、对象总数、空闲对象数
- **每个 slab 页头**：
  - 页类型、所属 class、对象大小
  - 本页对象总数、空闲对象数
  - 本页 freelist 头指针
  - 页链表前驱/后继指针

#### 7.3 分配与释放流程

- **分配**：
  1. 遍历 class 页链表，找 `free_objs > 0` 的页
  2. 若无，则 `grow_class()` 申请新页并加入链表
  3. 从目标页 freelist 弹出对象
- **释放**：
  1. 通过 `PGROUNDDOWN(p)` 定位页头
  2. 将对象插回该页 freelist
  3. 若该页 `free_objs == total_objs`，则：
     - 从 class 页链表中摘除
     - 更新 class 统计
     - 调用 `kfree()` 归还整页

#### 7.4 测试结果

```text
kmalloctest: cross-page growth ok (140 objects)
kmalloc stats:
  class size pages total free used
  0     32   0     0     0    0
  ... 全部为 0
  alloc_ok=148 alloc_fail=1 free_ok=148
  big_alloc_ok=3 big_free_ok=3 big_active_pages=0
  slab_pages_returned=4
  slab_pages=0 slab_objs=0 free_objs=0 used_objs=0
kmalloctest: ok
```

- `slab_pages_returned=4`：共有 4 个 slab 页归还给 `kfree()`
- 所有 class `pages=0`，`used_objs=0`：无资源泄漏

---

## 四、整体结论

当前 xv6 内核已具备 **两层内存管理能力**：

1. **页级**：`kalloc` / `kfree` 管理 4KB 物理页
2. **堆级**：`kmalloc` / `kmfree` 管理小块动态对象，支持：
   - 固定大小 slab 分配与释放
   - 单页大对象自动回退
   - 空 slab 页自动归还底层页分配器
   - 真实内核对象接入（`struct pipe`、`struct file`）

已通过 **启动自测 + 真实 shell 命令** 双重验证，系统稳定无 panic。

后续可继续扩展：
- 连续多页大对象分配
- 跨 class 页整理与合并
- 更细粒度的统计与调试接口
