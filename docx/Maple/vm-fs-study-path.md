# xv6-riscv 虚拟内存与文件系统学习分析路径

这份文档面向当前仓库中负责以下内容的学习与分析工作：

- 物理页分配
- 页表管理
- 用户地址空间
- 按需分页
- 文件系统
- inode 与目录
- buffer cache
- 日志
- 文件读写链路

目标不是把相关文件机械列出来，而是给出一条可以直接执行的阅读顺序、分析重点和调用链主线。

## 1. 先明确你负责的代码范围

### 1.1 虚拟内存主线

核心文件：

- `kernel/kalloc.c`
- `kernel/vm.c`
- `kernel/vm.h`
- `kernel/riscv.h`
- `kernel/memlayout.h`
- `kernel/proc.h`
- `kernel/proc.c`
- `kernel/trap.c`
- `kernel/trampoline.S`
- `kernel/kernelvec.S`
- `kernel/exec.c`
- `kernel/sysproc.c`

这些文件分别承担的角色：

- `kalloc.c`：物理页分配与回收
- `vm.c`：页表遍历、映射建立、用户地址空间创建/扩缩/复制/销毁
- `riscv.h`：Sv39、PTE、CSR 和地址转换相关底层定义
- `memlayout.h`：内核、设备、`TRAMPOLINE`、`TRAPFRAME` 的布局
- `proc.h` / `proc.c`：进程页表、trapframe、地址空间生命周期
- `trap.c` / `trampoline.S` / `kernelvec.S`：trap 入口、用户态返回、page fault 处理
- `exec.c`：把 ELF 程序装入新的用户地址空间
- `sysproc.c`：`sbrk` 等会直接影响用户空间布局的系统调用

### 1.2 文件系统主线

核心文件：

- `kernel/fs.h`
- `kernel/file.h`
- `kernel/buf.h`
- `kernel/bio.c`
- `kernel/log.c`
- `kernel/fs.c`
- `kernel/file.c`
- `kernel/sysfile.c`
- `kernel/virtio.h`
- `kernel/virtio_disk.c`
- `mkfs/mkfs.c`

配套文件：

- `kernel/stat.h`
- `kernel/fcntl.h`
- `kernel/sleeplock.h`
- `kernel/sleeplock.c`
- `kernel/spinlock.h`
- `kernel/spinlock.c`
- `kernel/param.h`
- `kernel/defs.h`

这些文件分别承担的角色：

- `fs.h`：磁盘文件系统格式定义
- `buf.h` / `bio.c`：块缓存层
- `log.c`：日志事务层
- `fs.c`：inode、目录、路径解析、数据块映射
- `file.h` / `file.c`：打开文件对象抽象
- `sysfile.c`：文件相关系统调用入口
- `virtio_disk.c` / `virtio.h`：磁盘设备驱动
- `mkfs.c`：文件系统镜像布局的构建工具

## 2. 推荐阅读顺序

### 2.1 虚拟内存路线

推荐顺序：

1. `kernel/riscv.h`
2. `kernel/memlayout.h`
3. `kernel/kalloc.c`
4. `kernel/vm.c`
5. `kernel/proc.h`
6. `kernel/proc.c`
7. `kernel/trampoline.S`
8. `kernel/trap.c`
9. `kernel/exec.c`
10. `kernel/sysproc.c`

这样排的原因：

- 不先理解 Sv39、PTE 和地址布局，后面读 `vm.c` 会很吃力
- 不先理解 `kalloc()`，就很难看清页表页和用户物理页从哪里来
- `proc.c` 把页表、trapframe、进程生命周期连在一起
- `trap.c` 决定了系统调用、异常和按需分页如何落地
- `exec.c` 解释用户地址空间如何真正装载程序
- `sysproc.c` 负责把 `sbrk` 这类用户接口接到地址空间变化上

### 2.2 文件系统路线

推荐顺序：

1. `kernel/fs.h`
2. `mkfs/mkfs.c`
3. `kernel/buf.h`
4. `kernel/bio.c`
5. `kernel/log.c`
6. `kernel/file.h`
7. `kernel/fs.c`
8. `kernel/file.c`
9. `kernel/sysfile.c`
10. `kernel/virtio.h`
11. `kernel/virtio_disk.c`

这样排的原因：

- `fs.h` 和 `mkfs.c` 先告诉你磁盘上到底长什么样
- `bio.c` 与 `log.c` 先建立“块缓存 + 日志提交”这两个底层机制
- `fs.c` 才是 inode、目录和路径名处理的核心
- `file.c` 把 inode、pipe、device 封装成统一的打开文件接口
- `sysfile.c` 再把用户态文件系统调用接入内核
- 最后回到底层设备驱动，看磁盘 I/O 如何真正发出去

## 3. 每条路线应该重点回答什么

### 3.1 虚拟内存路线的核心问题

你至少要能讲清：

- `kalloc()` 分配的是什么，谁会调用它
- xv6 的页表是几级结构，`walk()` 怎么逐级找到 PTE
- `mappages()` 怎样建立 VA -> PA 映射
- 用户页表和内核页表分别在什么时候建立
- `fork()` 如何复制父进程地址空间
- `exec()` 如何丢掉旧地址空间并装入新程序
- `TRAMPOLINE` 和 `TRAPFRAME` 为什么必须固定在高地址
- `usertrap()` 怎样处理系统调用、设备中断和 page fault
- 你当前仓库里的按需分页是怎样通过 page fault 补页的

建议重点跟踪这些函数：

- `kalloc`, `kfree`
- `walk`, `walkaddr`, `mappages`
- `uvmcreate`, `uvmalloc`, `uvmdealloc`, `uvmcopy`, `uvmfree`, `uvmunmap`
- `proc_pagetable`, `proc_freepagetable`
- `usertrap`, `usertrapret`
- `exec`, `growproc`, `fork`

### 3.2 文件系统路线的核心问题

你至少要能讲清：

- 文件系统镜像由哪些区域组成
- buffer cache 解决了什么问题
- `begin_op()` / `end_op()` 为什么必须包住文件系统修改
- inode 的磁盘结构和内存结构有什么区别
- `bmap()` 如何把文件逻辑块号翻译成磁盘块号
- `namei()` 如何从路径解析到目标 inode
- `struct file` 和 `struct inode` 的职责边界是什么
- `read()` 和 `write()` 从系统调用到磁盘 I/O 的链路是什么
- 删除文件为什么会涉及 `nlink` 和 inode 回收

建议重点跟踪这些函数：

- `bread`, `bwrite`, `brelse`
- `begin_op`, `end_op`, `log_write`
- `balloc`, `bmap`, `ialloc`, `iget`, `ilock`, `iput`
- `readi`, `writei`, `dirlookup`, `dirlink`, `namei`, `nameiparent`
- `filealloc`, `fileread`, `filewrite`
- `sys_open`, `sys_read`, `sys_write`, `sys_link`, `sys_unlink`
- `virtio_disk_rw`, `virtio_disk_intr`

## 4. 推荐按调用链学习

### 4.1 虚拟内存：用户空间增长与缺页补页

建议主线：

1. `sys_sbrk`
2. `growproc`
3. `usertrap`
4. `walk` / `walkaddr`
5. `kalloc`
6. `mappages`

要看清的问题：

- `sbrk` 调整的是“进程逻辑大小”还是“立即分配物理页”
- 什么情况下访问用户地址会触发 page fault
- page fault 后内核如何判断这是合法的延迟分配还是非法访问
- 新页分配后如何被挂到当前进程页表
- `fork`、`exit`、`exec` 如何与这些延迟分配页面配合

推荐自己画出这条链：

`sys_sbrk -> p->sz 变化 -> 首次访问新地址 -> trap -> page fault -> kalloc -> mappages -> 返回用户态`

### 4.2 文件系统：一次读操作

建议主线：

1. `sys_read`
2. `fileread`
3. `readi`
4. `bmap`
5. `bread`
6. `virtio_disk_rw`

要看清的问题：

- 用户传入的 fd 如何找到 `struct file`
- 普通文件和设备文件在 `fileread` 中如何分流
- `readi` 怎样按偏移定位到块
- `bread` 拿到的是缓存块还是新发起的磁盘读取

### 4.3 文件系统：一次写操作

建议主线：

1. `sys_write`
2. `filewrite`
3. `begin_op`
4. `writei`
5. `bmap` / `balloc`
6. `log_write`
7. `end_op`
8. `commit`

要看清的问题：

- 为什么写路径必须进日志事务
- 文件扩容时怎样分配新数据块
- 数据块和 inode 元数据分别在什么时候落盘
- 日志提交和 buffer cache 是怎样配合的

### 4.4 文件系统：路径解析与目录操作

建议主线：

1. `sys_open` 或 `sys_unlink`
2. `namei` / `nameiparent`
3. `namex`
4. `dirlookup`
5. `dirlink`
6. `ialloc` / `iput` / `itrunc`

要看清的问题：

- 路径是怎样按目录层层向下查找的
- 创建文件时目录项和 inode 谁先建立
- 删除文件时目录项删除、`nlink` 减少、inode 真正释放之间是什么关系

## 5. 建议产出物

如果你是按学习/汇报方式推进，这部分内容建议最终整理成 4 张图和 2 条链：

### 图

- 内核地址空间布局图
- 单个用户进程地址空间图
- 文件系统磁盘布局图
- 文件系统分层图：`syscall -> file -> inode -> log -> bio -> disk`

### 链

- 缺页补页调用链
- 文件读写调用链

## 6. 最后检查自己是否真的学会

如果你已经吃透这一块，应该能不用看代码直接口述下面这些问题：

- `kalloc` 和 `uvmalloc` 的关系是什么
- `walk`、`mappages`、`walkaddr` 分别负责哪一层抽象
- `fork` 时用户地址空间是如何复制的
- `exec` 为什么能“换程序不换进程”
- `TRAMPOLINE` 和 `TRAPFRAME` 为什么要固定映射
- `read()` 一次调用是怎样一路走到磁盘块的
- `write()` 为什么不是直接 `bwrite()`，而要经过日志层
- `namei()` 为什么是理解文件系统的关键入口

## 7. 结合当前仓库的建议切入点

如果你现在就开始推进，建议按下面节奏执行：

1. 先用 1 天把 `riscv.h + memlayout.h + kalloc.c + vm.c` 串起来
2. 再用 1 天把 `proc.c + trap.c + trampoline.S + exec.c` 串成地址空间生命周期
3. 然后用 1 天把 `fs.h + mkfs.c + bio.c + log.c + fs.c` 串成文件系统主干
4. 最后用 1 天把 `file.c + sysfile.c + virtio_disk.c` 串成读写调用链

如果时间更紧，最优先的最小闭环是：

- 虚拟内存：`kalloc.c -> vm.c -> trap.c`
- 文件系统：`fs.c -> file.c -> sysfile.c -> log.c -> bio.c`

