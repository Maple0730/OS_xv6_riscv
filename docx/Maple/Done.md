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

## 三、物理页框分配器与课程指标对照

对应主要文件：  
`kernel/kalloc.c`, `kernel/memlayout.h`, `kernel/riscv.h`, `kernel/sysproc.c`, `kernel/vm.c`, `kernel/trap.c`, `kernel/proc.c`, `kernel/shm.c`

### 1. 当前物理页框分配器是什么

当前 xv6 底层的物理页框分配器不是位图，也不是伙伴算法，而是**基于空闲链表的简单页分配器**。

核心结构与行为：
- 使用 `struct run { struct run *next; }` 表示一个空闲物理页
- 通过 `kmem.freelist` 维护所有空闲页的单链表
- `kinit()` 将 `end ~ PHYSTOP` 之间的可用物理页逐页加入空闲链表
- `kalloc()` 从链表头取出一个 4KB 页返回
- `kfree()` 将释放的页重新插回链表头

这说明当前实现已经满足“**基于链表的简单物理页框分配器**”这一技术路线。

### 2. 指标一：支持至少 4MB 物理内存管理

当前实现**满足**这一指标，而且实际支持范围远大于 4MB。

依据：
- `KERNBASE = 0x80000000`
- `PHYSTOP = KERNBASE + 128 * 1024 * 1024`
- 即当前系统按 `128MB` RAM 上限组织物理页管理

实现方式：
- `kinit()` 调用 `freerange(end, (void *)PHYSTOP)`
- 将内核镜像结尾 `end` 到 `PHYSTOP` 之间的物理页加入页分配器

因此，从课程指标角度看，**“至少 4MB 物理内存管理”已经覆盖且留有充足余量**。

### 3. 指标二：页大小为 4KB

当前实现**满足**这一指标。

依据：
- `PGSIZE` 明确定义为 `4096`
- `kalloc()` / `kfree()` 的分配与回收单位都是整页
- 页表映射、地址对齐、缺页补页等路径也统一以 `PGSIZE` 为基本粒度

也就是说，当前系统的底层内存管理单位就是**4KB 物理页框**。

### 4. 指标三：支持按需分页（Demand Paging）

当前实现**基本满足**这一指标，但更准确地说，是实现了**基于 lazy `sbrk` 的按需分配 / 缺页补页机制**。

实现路径：
- 用户调用 `sbrk(..., SBRK_LAZY)` 时，`sys_sbrk()` 仅增加 `p->sz`
- 此时并不会立即调用 `kalloc()` 分配物理页
- 当进程首次访问该虚拟页时，触发 page fault
- `usertrap()` 检测到缺页异常后调用 `vmfault()`
- `vmfault()` 再执行：
  - `kalloc()` 分配物理页
  - `memset()` 清零
  - `mappages()` 建立映射

因此，当前系统已经具备：
- **逻辑地址空间先扩张**
- **物理页在首次访问时再补齐**

这正是教学型 xv6 中常见的 Demand Paging 实现方式。

**需要注意**：
- 当前并未实现磁盘换页、页置换算法、swap 分区等完整虚拟内存子系统
- 但就课程通常要求的“缺页触发后按需分配物理页”而言，当前实现已经成立

### 5. 指标四：内存分配无泄漏，支持释放后重用

这个指标需要分开说明。

#### 5.1 释放后重用

当前实现**满足**。

原因：
- `kfree()` 会把释放的物理页重新挂回 `kmem.freelist`
- 后续 `kalloc()` 可以再次从链表中取出这些页
- 普通用户页在 `uvmunmap()` / `uvmfree()` / `proc_freepagetable()` 路径中会被回收

因此，底层页框具备明确的**释放后复用能力**。

#### 5.2 是否可以严格宣称“无泄漏”

对于**普通用户页、页表页、trapframe 页**这类主路径资源，可以认为已经具备完整回收链路：
- 进程退出时通过 `proc_freepagetable()` 释放固定映射与用户页
- `uvmfree()` 会回收用户页并递归释放页表页
- `kfree()` 最终将物理页归还到底层空闲链表

但是，当前仓库中新增的**共享内存 `shm` 扩展路径**还存在一个严格性问题：
- `shmdt()` 会递减引用计数，并在归零时 `kfree()` 共享页
- 但 `freeproc()` 在进程退出时只会摘掉 `SHM_BASE` 的映射，**不会**递减共享页引用计数
- 如果某进程异常退出或退出前未显式 `shmdt()`，该共享页可能无法及时回收

因此，更严谨的结论应写为：
- **普通页分配/回收主路径已经支持释放后重用，整体机制基本完整**
- **若把 `shm` 扩展路径也纳入统一考核，则当前版本还不能严格宣称“所有内存路径绝对无泄漏”**

### 6. 小结

> 当前系统已经实现基于空闲链表的 4KB 物理页框分配器，可管理 `end ~ PHYSTOP` 范围内的物理内存，实际支持规模达到 128MB，明显超过课程要求的 4MB；同时，系统已支持基于 lazy `sbrk` 的按需分页（缺页后分配物理页）机制。对于普通页分配路径，系统支持释放与重用；但若将扩展的共享内存 `shm` 路径一并纳入，则当前版本尚不宜严格宣称“全路径绝对无泄漏”。

### 7. 可直接用于报告的结论

> 当前 xv6 已实现基于空闲链表的简单物理页框分配器，分配粒度为 4KB 页。系统启动时会将内核镜像结尾 `end` 到 `PHYSTOP` 之间的可用物理内存加入空闲页链表，当前配置下可管理的物理内存规模达到 128MB，满足并超过“至少 4MB”要求。同时，系统支持基于 lazy `sbrk` 的按需分页：在扩展进程地址空间时仅增加逻辑大小，首次访问未映射页时由缺页异常触发 `kalloc + mappages` 完成物理页分配与映射。对于普通页分配路径，系统已支持释放后重用；但若将共享内存扩展路径计入统一考核，则当前版本对“无泄漏”这一指标仍应保留严格性说明。

---

## 四、内核堆内存分配（`kmalloc`/`kmfree`）

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

## 五、整体结论

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

---

# 文件系统

## 一、文件描述符表管理

对应主要文件：  
`kernel/proc.h`, `kernel/sysfile.c`, `kernel/file.c`, `kernel/file.h`

### 1. 基本含义

文件描述符表管理的核心作用是：
- 为每个进程维护一张“文件描述符整数 `fd` → 内核打开文件对象”的映射表
- 支持文件描述符的分配、查找、复制、关闭与回收
- 支持 `fork` 后父子进程共享已打开文件
- 支持进程退出时自动关闭全部打开文件

当前 xv6 已经实现这一机制，不只是支持 `open/read/write/close` 这些系统调用表面行为，而是已经具备完整的底层数据结构和生命周期管理。

### 2. 两层结构设计

当前实现采用经典的两层结构：

1. **每进程文件描述符表**
   - 每个进程在 `struct proc` 中维护 `ofile[NOFILE]`
   - 数组下标就是用户看到的文件描述符 `fd`
   - `ofile[fd]` 指向对应的内核 `struct file`

2. **全局打开文件对象**
   - `struct file` 表示一个真正“已打开”的文件对象
   - 其中保存：
     - 文件类型（普通文件 / 设备 / 管道）
     - 引用计数 `ref`
     - 可读写权限
     - 当前读写偏移 `off`
     - 对应的 inode / pipe 指针

也就是说：
- 用户态看到的是整数 fd
- 内核态真正管理的是 `struct file`

### 3. 主要实现机制

#### 3.1 文件描述符分配

- `open()`、`pipe()` 等系统调用在创建好 `struct file` 后
- 会调用 `fdalloc()` 在当前进程 `ofile[]` 中查找第一个空槽
- 找到后把该 `struct file *` 挂到这个槽位上
- 返回该数组下标作为新的文件描述符

因此，文件描述符分配本质上就是：
- 在进程自己的文件描述符表里找空位
- 建立 `fd -> struct file` 的映射

#### 3.2 文件描述符查找

- `read()`、`write()`、`close()`、`fstat()` 等系统调用
- 先通过 `argfd()` 检查 fd 是否越界、是否已打开
- 再从当前进程 `ofile[fd]` 中取出对应 `struct file *`

这保证了：
- 非法 fd 会直接失败
- 合法 fd 能稳定定位到对应打开文件对象

#### 3.3 `dup` 复制

- `dup(fd)` 不会新建底层文件
- 而是为同一个 `struct file` 再分配一个新的 fd
- 同时调用 `filedup()` 增加引用计数

因此，多个 fd 可以共享同一个打开文件对象，并共享其读写偏移和状态。

#### 3.4 `close` 关闭

- `close(fd)` 先将当前进程 `ofile[fd]` 置空
- 再调用 `fileclose()` 给底层 `struct file` 的引用计数减一
- 只有当引用计数降为 `0` 时，才真正释放对应资源：
  - pipe 对象
  - inode 引用
  - `struct file` 本身

这说明当前实现不是“简单删掉一个数组项”，而是完整的引用计数回收机制。

### 4. 与进程生命周期的配合

#### 4.1 `fork` 后继承文件描述符

- 在 `fork` 中，子进程会遍历父进程的 `ofile[]`
- 对每个非空项调用 `filedup()`
- 从而让父子进程共享同一组已打开文件对象

这意味着：
- `fork` 后子进程继承父进程的打开文件
- 共享底层 `struct file`
- 通过引用计数保证生命周期正确

#### 4.2 `exit` 时自动关闭

- 进程退出时会遍历自己的 `ofile[]`
- 对每个非空文件描述符调用 `fileclose()`
- 然后将对应槽位清空

因此，当前系统已经支持：
- 进程结束后自动回收其持有的全部打开文件资源
- 避免文件描述符泄漏

### 5. 当前已实现的能力总结

当前 xv6 已经实现：
- 每进程独立文件描述符表
- `fd` 到 `struct file` 的映射
- 文件描述符分配与回收
- `read/write/close/fstat` 的 fd 查找路径
- `dup` 引用共享
- `fork` 后文件描述符继承
- `exit` 时自动关闭全部打开文件
- 基于引用计数的底层打开文件对象管理

### 6. 可直接用于报告的结论

> 当前系统已经实现文件描述符表管理机制。每个进程维护独立的文件描述符表 `ofile[NOFILE]`，用于建立用户态文件描述符整数到内核 `struct file` 对象的映射。系统支持文件描述符的分配、查找、复制与关闭：`open/pipe` 等操作可分配新的 fd，`read/write/fstat` 可通过 fd 定位对应打开文件对象，`dup` 可共享同一文件对象并增加引用计数，`close` 会在清除进程表项后递减引用计数，并在引用归零时释放底层资源。同时，`fork` 会继承父进程的文件描述符表并同步增加引用计数，`exit` 会自动关闭进程持有的全部文件描述符。因此，当前 xv6 已具备完整的基础文件描述符表管理能力。

## 二、文件/目录的创建、删除、打开、关闭、读写

对应主要文件：  
`kernel/sysfile.c`, `kernel/file.c`, `kernel/fs.c`

### 1. 总体说明

当前系统已经实现普通文件和目录的基础操作能力，包括：
- 创建
- 删除
- 打开
- 关闭
- 读
- 写

这些能力并不是由某一个函数独立完成，而是由三层共同配合实现：
- **系统调用层 `sysfile`**：负责参数提取、合法性检查、事务边界控制
- **打开文件对象层 `file`**：负责 `struct file`、读写权限、引用计数与当前偏移
- **文件系统核心层 `fs`**：负责 inode、目录项、路径解析、块分配和实际数据读写

### 2. 文件/目录创建

#### 2.1 普通文件创建

普通文件创建主要通过：
- `open(path, O_CREATE, ...)`

在 `sys_open()` 中：
- 若打开标志包含 `O_CREATE`
- 则调用内部 `create(path, T_FILE, 0, 0)`

`create()` 的核心工作包括：
- `nameiparent()` 找到父目录
- 检查目标名字是否已经存在
- `ialloc()` 分配新的 inode
- 初始化 inode 元数据
- `dirlink()` 将新名字写入父目录

因此，当前系统已经支持普通文件创建。

#### 2.2 目录创建

目录创建通过：
- `mkdir(path)`

在 `sys_mkdir()` 中同样调用：
- `create(path, T_DIR, 0, 0)`

与普通文件不同的是，目录创建时还会额外建立：
- `.`
- `..`

因此，当前系统已经支持目录创建。

### 3. 文件/目录删除

删除主要通过：
- `unlink(path)`

其核心流程为：
- `nameiparent()` 找到父目录
- 禁止删除 `.` 和 `..`
- `dirlookup()` 找到目标 inode
- 若目标是目录，则必须先检查是否为空目录
- 将父目录中对应目录项清零
- 递减目标 inode 的 `nlink`
- 最终由 `iput()` 在合适时机回收 inode 及其数据块

因此，当前系统已经支持：
- 普通文件删除
- 空目录删除

### 4. 文件/目录打开与关闭

#### 4.1 打开

打开通过：
- `open(path, omode)`

完成过程包括：
- `namei()` 或 `create()` 获取目标 inode
- `filealloc()` 分配新的 `struct file`
- `fdalloc()` 分配文件描述符
- 初始化打开对象的：
  - 类型
  - 可读写权限
  - 当前偏移 `off`
  - 对应 inode 指针

目录也支持打开，但通常只允许只读方式打开。

#### 4.2 关闭

关闭通过：
- `close(fd)`

完成过程包括：
- 从当前进程 `ofile[]` 中清除该 fd
- 调用 `fileclose()` 递减底层 `struct file` 引用计数
- 若引用计数归零，再真正释放：
  - pipe
  - inode 引用
  - `struct file` 本身

因此，当前系统已经实现完整的打开/关闭机制，而不是简单的数组删除。

### 5. 文件读写

#### 5.1 读

读取通过：
- `read(fd, buf, n)`

其路径为：
- `sys_read()`
- `fileread()`
- `readi()`

对于普通文件：
- 通过 `struct file` 的当前偏移 `off`
- 调用 inode 层 `readi()` 从对应数据块中读出内容
- 成功后推进 `f->off`

因此，当前系统已经支持普通文件顺序读取。

#### 5.2 写

写入通过：
- `write(fd, buf, n)`

其路径为：
- `sys_write()`
- `filewrite()`
- `writei()`

对于普通文件：
- 按当前偏移写入数据
- 必要时通过 `bmap()` / `balloc()` 分配新的数据块
- 使用 `log_write()` 将修改纳入日志事务
- 更新文件大小与 inode 元数据
- 成功后推进 `f->off`

因此，当前系统已经支持普通文件顺序写入，并具备日志一致性保护。

### 6. 目录操作的补充说明

目录虽然也属于 inode 文件系统的一部分，但它不是供用户随意按普通文本文件方式写入的对象。

当前目录支持的能力包括：
- 创建目录
- 删除空目录
- 打开目录
- 关闭目录
- 路径遍历
- 目录项查找与插入

其中目录内容的维护主要通过内核内部函数完成：
- `dirlookup()`
- `dirlink()`
- `namei()`
- `nameiparent()`

因此，从作业要求角度看，目录相关操作能力已经实现。

### 7. 可直接用于报告的结论

> 当前系统已经实现文件和目录的创建、删除、打开、关闭、读写能力。普通文件创建通过 `open(path, O_CREATE)` 与 `create()` 完成，目录创建通过 `mkdir()` 与 `create(T_DIR)` 完成；删除通过 `unlink()` 删除目录项并递减 inode 链接计数；打开通过 `namei()` / `create()` 获取 inode、再分配 `struct file` 与文件描述符；关闭通过 `close()` 与 `fileclose()` 完成引用计数回收；读写分别通过 `fileread()` / `filewrite()` 调用 inode 层 `readi()` / `writei()` 实现，并由日志层保证多块修改的一致性。因此，该项要求已经完成。

## 三、路径解析（绝对路径与相对路径）

对应主要文件：  
`kernel/fs.c`, `kernel/sysfile.c`

### 1. 总体说明

当前系统已经实现路径解析能力，并同时支持：
- **绝对路径**
- **相对路径**

这部分能力的核心位于 `fs` 层：
- `namei(path)`
- `nameiparent(path, name)`

它们又统一建立在内部函数：
- `namex(path, nameiparent, name)`

之上。

### 2. 最底层辅助：`skipelem()`

路径解析最底层的字符串处理函数是：
- `skipelem(path, name)`

其作用是：
- 从路径字符串中提取下一段路径元素
- 自动跳过多余的 `/`

例如：
- `/a/b/c` 依次解析为 `a`、`b`、`c`
- `///a//b` 依然能正确解析为 `a`、`b`

因此，`skipelem()` 负责完成“路径分段”，但它本身并不查目录，也不解析 inode。

### 3. 核心解析函数：`namex()`

真正完成路径遍历的是：
- `namex(path, nameiparent, name)`

其主要逻辑为：

1. **决定起点**
   - 若路径以 `/` 开头，则从根目录 `ROOTINO` 开始
   - 否则从当前进程的工作目录 `cwd` 开始

2. **逐段解析**
   - 通过 `skipelem()` 取出下一段名字
   - 锁住当前 inode
   - 检查当前 inode 必须是目录
   - 调用 `dirlookup()` 在当前目录中查找下一层名字
   - 找到后继续向下遍历

3. **根据需求返回结果**
   - 若要求返回目标对象本身，则一直走到最后
   - 若要求返回父目录，则在最后一层之前停止

因此，路径解析的本质就是：
- 当前目录 inode
- 加上下一段名字
- 经过 `dirlookup()`
- 得到下一层 inode

不断重复直到路径结束。

### 4. 绝对路径与相对路径的实现方式

#### 4.1 绝对路径

若路径以 `/` 开头，例如：
- `/a/b/c`

则 `namex()` 会从：
- 根目录 inode `ROOTINO`

开始解析。

因此，绝对路径解析能力已经实现。

#### 4.2 相对路径

若路径不以 `/` 开头，例如：
- `a/b/c`

则 `namex()` 会从：
- 当前进程的工作目录 `myproc()->cwd`

开始解析。

因此，相对路径解析能力也已经实现。

### 5. `namei()` 与 `nameiparent()` 的分工

#### 5.1 `namei(path)`

`namei()` 的作用是：
- 解析完整路径
- 返回目标对象本身的 inode

例如：
- `namei("/a/b/c")`
- 返回的是 `c` 的 inode

它主要用于：
- `sys_open()` 打开已有文件
- `sys_chdir()` 切换目录
- `sys_link()` 查找旧路径
- `exec()` 查找可执行文件

#### 5.2 `nameiparent(path, name)`

`nameiparent()` 的作用是：
- 解析到目标对象的父目录
- 同时把最后一段名字写入 `name`

例如：
- `nameiparent("/a/b/c", name)`
- 返回的是目录 `b` 的 inode
- `name` 中得到 `"c"`

它主要用于：
- `create()` 创建文件或目录
- `sys_unlink()` 删除文件或目录项
- `sys_link()` 建立新硬链接

因为这些操作都不是单纯“找到目标对象”，而是要修改其父目录。

### 6. 向上的封装路径

在 `fs` 层完成路径解析后，其上层主要封装在：
- `sys_open()`
- `sys_unlink()`
- `sys_link()`
- `sys_chdir()`
- `create()`

这些函数中。

典型调用关系为：

- **打开已有文件**
  - `sys_open()` → `namei()`

- **创建普通文件**
  - `sys_open(O_CREATE)` → `create()` → `nameiparent()`

- **创建目录**
  - `sys_mkdir()` → `create()` → `nameiparent()`

- **删除文件/目录项**
  - `sys_unlink()` → `nameiparent()`

- **切换当前目录**
  - `sys_chdir()` → `namei()`

因此，`namei()` 和 `nameiparent()` 已经被完整封装到系统调用路径之中，并实际支撑文件系统各类用户接口。

### 7. 可直接用于报告的结论

> 当前系统已经实现路径解析能力，支持绝对路径和相对路径。其核心位于 `fs` 层的 `namei()` 与 `nameiparent()`：二者都基于内部函数 `namex()` 实现，`namex()` 通过 `skipelem()` 将路径分段，并在目录 inode 上逐层调用 `dirlookup()` 完成路径遍历。若路径以 `/` 开头，则从根目录 `ROOTINO` 开始解析；否则从当前进程的工作目录 `cwd` 开始解析，因此同时支持绝对路径和相对路径。其上层由 `sysfile.c` 中的 `sys_open()`、`sys_unlink()`、`sys_link()`、`sys_chdir()` 以及内部 `create()` 等函数封装使用：`namei()` 用于获取目标对象 inode，`nameiparent()` 用于获取父目录 inode 与最终名字，从而支撑文件/目录的创建、删除、打开和工作目录切换等操作。

## 四、文件系统整体总结

当前项目中的文件系统并不是标准 `ext2`/`ext3` 的直接实现，而是 **xv6 风格的简化类 Unix inode 磁盘文件系统**。其整体实现已经形成较清晰的分层结构，并能够从底层磁盘块访问一直支撑到用户态文件系统接口。

从下到上看，当前实现主要包括以下几个层次：

1. **块设备层**
   - 通过 VirtIO 块设备完成对磁盘镜像 `fs.img` 的底层块读写
   - 为上层文件系统提供最基础的块设备访问能力

2. **块缓存层（`bio`）**
   - 将磁盘块缓存到内存中的 `struct buf`
   - 提供 `bread`、`bwrite`、`brelse` 等统一接口
   - 负责块级缓存复用与并发访问同步

3. **日志层（`log`）**
   - 实现简化的 write-ahead logging / physical redo log
   - 通过 `begin_op` / `end_op` / `log_write` / `commit`
   - 保证多块元数据更新在崩溃场景下仍能恢复一致性

4. **文件系统核心层（`fs`）**
   - 实现 superblock 管理
   - 实现空闲块位图分配与释放（`balloc` / `bfree`）
   - 实现 inode 的分配、缓存、装载、写回与回收（`ialloc` / `iget` / `ilock` / `iupdate` / `iput`）
   - 实现文件数据块映射与截断（`bmap` / `itrunc`）
   - 实现基于 inode 的文件内容读写（`readi` / `writei`）
   - 实现目录项管理与路径解析（`dirlookup` / `dirlink` / `namei` / `nameiparent`）

5. **打开文件对象层（`file`）**
   - 用 `struct file` 表示一次 `open()` 之后的运行时打开文件实例
   - 维护当前读写偏移、读写权限和引用计数
   - 区分普通文件、设备文件和管道三类打开对象
   - 当前仓库中 `struct file` 已改为动态分配对象

6. **进程文件描述符层**
   - 每个进程维护 `ofile[NOFILE]` 文件描述符表
   - 建立 `fd -> struct file` 的映射关系
   - 支持 `dup`、`fork` 继承、`exit` 自动关闭等完整生命周期管理

7. **系统调用接口层（`sysfile`）**
   - 向用户态封装 `open`、`read`、`write`、`close`
   - 以及 `link`、`unlink`、`mkdir`、`chdir`、`pipe` 等接口
   - 使用户程序可以通过标准文件系统调用访问内核文件系统能力

### 总结结论

> 当前系统已经实现一套完整的简化类 Unix inode 文件系统。其实现覆盖了从底层磁盘块读写、块缓存、日志一致性保护，到 inode/目录/路径管理、打开文件对象管理、进程文件描述符表管理，再到用户态系统调用接口的完整链路。该文件系统已能够支持普通文件与目录的创建、删除、打开、关闭、读写，以及路径解析、文件描述符映射和崩溃一致性保护，具备一个教学型操作系统文件系统的核心能力。

## 五、技术指标对应（已实现部分）

### 1. 支持至少 128 个文件

当前文件系统镜像构建工具 `mkfs` 中定义：
- `NINODES = 200`

并在初始化文件系统镜像时写入 superblock，因此从文件系统可容纳的 inode 数量来看，当前镜像已经能够支持超过 `128` 个文件/目录对象。

需要说明的是：
- 当前这里对应的是“文件系统中可创建/容纳的文件数量”
- 而不是“系统同时打开的文件描述符数量”

因此，从课程技术指标角度看，该项已满足。

### 2. 单文件最大 64KB

当前文件系统中：
- `BSIZE = 1024`
- `NDIRECT = 12`
- `NINDIRECT = BSIZE / sizeof(uint) = 256`
- `MAXFILE = NDIRECT + NINDIRECT = 268`

因此，单文件最大块数为 `268` 块，最大文件大小约为：

- `268 * 1024 = 274432` 字节

远大于 `64KB` 的要求。

同时，在 inode 写入路径 `writei()` 中，也通过：
- `off + n > MAXFILE * BSIZE`

对单文件最大长度进行边界控制。

因此，该项已满足。

### 3. 目录层级至少支持 3 层嵌套

当前系统的路径解析由：
- `skipelem()`
- `namex()`
- `namei()`
- `nameiparent()`

共同实现。

其中 `namex()` 不是写死目录层数，而是通过循环逐段解析路径名：
- 取出下一段路径元素
- 在当前目录中通过 `dirlookup()` 查找
- 找到后继续向下遍历

因此，目录层级支持并不限于 `3` 层，而是只要路径长度和目录结构合法，就能够持续向下解析。

从课程技术指标角度看：
- “至少支持 3 层嵌套目录”这一要求已经满足。

### 4. 提供 `mkfs` 工具初始化文件系统镜像

当前项目已经提供独立的 `mkfs` 工具：
- 在宿主机上创建并初始化 `fs.img`

其功能包括：
- 创建/截断文件系统镜像
- 写入 superblock
- 初始化 inode 区、位图区和数据区
- 创建根目录 inode
- 写入 `.` 和 `..`
- 将 README 与用户程序打包进入镜像

因此，当前系统已经具备完整的文件系统镜像初始化工具链，该项要求已满足。

### 5. 文件读写支持 `seek` 操作

当前系统已经补齐 `seek` 能力，采用的是一版最小可用、且与现有文件系统能力保持一致的 `lseek` 设计。

实现位置包括：
- `kernel/syscall.h`
  - 新增 `SYS_lseek`
- `kernel/syscall.c`
  - 注册 `sys_lseek`
- `kernel/sysfile.c`
  - 实现 `sys_lseek()`
- `kernel/file.c`
  - 实现 `fileseek()`
- `kernel/fcntl.h`
  - 新增 `SEEK_SET`、`SEEK_CUR`、`SEEK_END`
- `user/user.h`
  - 新增 `lseek()` 用户态声明
- `user/usys.pl`
  - 新增系统调用桩

当前语义如下：
- 只支持普通文件
- 对目录、管道、设备文件调用 `lseek` 返回 `-1`
- 支持：
  - `SEEK_SET`
  - `SEEK_CUR`
  - `SEEK_END`
- 成功时返回新的文件偏移
- 失败时返回 `-1`

当前边界约束如下：
- 新偏移不能小于 `0`
- 新偏移不能超过 `MAXFILE * BSIZE`
- 新偏移不能超过当前文件大小

这意味着：
- 可以定位到 `EOF`
- 不能定位到 `EOF` 之后

之所以这样设计，是因为当前 `writei()` 还不支持：
- 从 `EOF` 之后直接开始写入
- 自动为中间空洞补零

因此本实现选择让 `lseek()` 的接口语义与当前文件系统真实能力保持一致，而不是先暴露一个“能 seek 过去但后续 write 不完整”的半支持接口。

另外需要明确：
- `lseek()` 本身不做读写
- 它只修改打开文件对象 `struct file` 中的当前偏移 `off`
- 真正的读写仍然走 `read -> fileread()` 和 `write -> filewrite()`
- 这两个路径都会使用 `f->off`，并在成功后自动推进偏移

实际测试中已经验证：
- `SEEK_SET`
- `SEEK_CUR`
- `SEEK_END`
- 定位后的覆盖写
- `dup()` 后共享偏移
- 定位到 `EOF` 再读返回 `0`
- 继续定位到 `EOF` 之后返回 `-1`
- 非法 `whence`
- 负偏移失败
- `pipe` 上 `lseek` 失败

在 xv6 内实际运行 `seektest` 的结果为：
- `seektest: ok`

因此，课程要求中的“文件读写支持 seek 操作”这一项现在也已经完成。

### 6. 可直接用于报告的结论

> 从当前实现结果看，文件系统相关技术指标中，已经明确满足的部分包括：支持至少 128 个文件、单文件最大容量超过 64KB、目录层级支持至少 3 层嵌套、文件读写支持 `seek` 操作，以及提供 `mkfs` 工具初始化文件系统镜像。其中，文件数量能力由 `mkfs` 写入的 inode 规模保证，单文件容量由 `MAXFILE * BSIZE` 上限保证，多级目录能力由 `namex()` 的逐层路径解析保证，`seek` 能力由 `lseek -> sys_lseek -> fileseek` 这一链路保证，而文件系统镜像初始化则由独立的 `mkfs` 工具完成。
