# xv6-riscv 学习路线：从最小可运行内核到一个完整 OS

这份路线不是按源码目录平铺，而是按“系统怎样一步步活起来”组织。目标是先建立最小闭环，再把它扩展成一个完整 OS 的心智模型。

适用对象：

- 正在学操作系统，但发现老教程和当前 `xv6-riscv` 代码有差异
- 想按“从底向上”而不是“按模块背诵”理解 xv6
- 希望每一阶段都能知道自己到底应该看哪些文件、回答哪些问题

---

## 0. 先建立总目标

你最终要能独立讲清这条主线：

`QEMU 启动 -> 内核进入 C -> 建立页表和内存分配 -> 建立 trap 机制 -> 创建并调度第一个进程 -> 返回用户态 -> 启动 /init -> 启动 shell -> 通过系统调用访问文件和设备`

如果这条线打通了，再去看“六大管理功能”，你会感觉它们是在描述同一个系统的不同切面，而不是一堆散文件。

---

## 1. 先学什么是“最小可运行内核”

### 目标

先不要急着看进程、文件系统、设备。第一步只回答：

- 机器上电后，为什么能跑到内核代码？
- 为什么先是汇编，再是 C？
- 一个内核至少要具备什么，才能继续往下运行？

### 必读文件

- `kernel/entry.S`
- `kernel/start.c`
- `kernel/main.c`
- `kernel/kernel.ld`
- `kernel/memlayout.h`

### 这一阶段必须搞懂

- `entry.S` 负责把 CPU 带进内核最早期执行环境
- `start()` 仍然运行在 machine mode，负责切到 supervisor mode
- `main()` 才是大部分内核初始化开始的地方
- `kernel.ld` 决定内核镜像如何被链接和放置
- `memlayout.h` 给出硬件地址、RAM 范围、trampoline 等关键布局

### 建议输出

- 画出启动链：`entry -> start -> main`
- 画出 CPU 0 和其他 CPU 的初始化分工
- 用一句话解释为什么 `main()` 还不是“系统已经完全可用”

### 自测问题

- 为什么 `start()` 里要设置 `mstatus`、`mepc`、`satp`？
- 为什么只有 `cpuid() == 0` 的 CPU 执行大部分全局初始化？
- `main()` 结束时为什么所有 CPU 都进入 `scheduler()`？

---

## 2. 再学内核赖以存在的最小内存系统

### 目标

理解内核为什么能“安全访问内存”，以及用户进程为什么能有自己的地址空间。

### 必读文件

- `kernel/kalloc.c`
- `kernel/vm.c`
- `kernel/vm.h`
- `kernel/memlayout.h`
- `kernel/riscv.h`

### 这一阶段必须搞懂

- `kalloc.c`：物理页分配器
- `vm.c`：页表建立、地址映射、用户地址空间辅助函数
- `riscv.h`：PTE、寄存器、Sv39 相关常量和辅助宏
- `memlayout.h`：内核和设备的地址布局

### 重点概念

- 物理内存和虚拟内存不是一回事
- xv6-riscv 采用页表来做地址翻译
- 内核对一段物理内存做了直接映射
- `TRAMPOLINE` 和 `TRAPFRAME` 是后面 trap 机制的关键准备

### 建议输出

- 画出内核地址空间图
- 画出用户地址空间图
- 列出 `KERNBASE`、`PHYSTOP`、`TRAMPOLINE`、`TRAPFRAME` 的作用

### 自测问题

- `kalloc()` 分配的到底是什么？
- `kvminit()` 和 `kvminithart()` 分别做什么？
- 为什么 trampoline 要放在高地址固定位置？

---

## 3. 学 CPU 如何在用户态和内核态之间切换

### 目标

理解系统调用、异常、中断的统一入口，以及为什么返回用户态这么麻烦。

### 必读文件

- `kernel/trap.c`
- `kernel/kernelvec.S`
- `kernel/trampoline.S`

### 这一阶段必须搞懂

- 用户态 trap 进入内核时发生了什么
- 内核态 trap 发生时走哪条路径
- trapframe 保存的是用户寄存器现场
- 返回用户态前为什么要重新配置 `stvec`、`sstatus`、`sepc`

### 重点概念

- `usertrap()`
- `kerneltrap()`
- `prepare_return()`
- `uservec`
- `userret`

### 建议输出

- 画出一次系统调用链路：`ecall -> usertrap -> syscall -> prepare_return -> userret`
- 区分 `trapframe` 和 `context`

### 自测问题

- 为什么 `usertrap()` 一进来先把 `stvec` 改成 `kernelvec`？
- 为什么 `prepare_return()` 里要设置 `kernel_satp`、`kernel_sp`？
- 为什么从内核返回用户态不能只靠普通的 C 返回？

---

## 4. 学最小进程系统和调度器

### 目标

理解 OS 怎么抽象“可运行的执行单元”，以及 CPU 怎么在不同进程之间切换。

### 必读文件

- `kernel/proc.h`
- `kernel/proc.c`
- `kernel/swtch.S`
- `kernel/spinlock.c`
- `kernel/sleeplock.c`

### 这一阶段必须搞懂

- `struct proc` 中哪些字段是最核心的
- `allocproc()` 怎样造出一个“可用但还没真的运行”的进程
- `scheduler()` 如何选中 `RUNNABLE` 进程
- `sched()` 和 `yield()` 的职责边界
- `swtch()` 保存和恢复的是内核上下文

### 必抓主线函数

- `allocproc`
- `userinit`
- `scheduler`
- `sched`
- `yield`
- `sleep`
- `wakeup`
- `forkret`

### 建议输出

- 画出进程状态机
- 画出 `scheduler <-> swtch <-> 进程内核上下文` 的切换图
- 解释为什么 `allocproc()` 返回时还持有 `p->lock`

### 自测问题

- `context` 和 `trapframe` 分别保存什么？
- 为什么新进程第一次运行要从 `forkret()` 开始？
- `sleep/wakeup` 为什么必须围绕锁来设计？

---

## 5. 打通“第一个用户程序怎么起来”

### 目标

把这份仓库和老教程的差异彻底搞明白。

### 必读文件

- `kernel/proc.c`
- `kernel/exec.c`
- `user/init.c`

### 你这份代码和老教程的关键差异

老教程常说第一个用户程序通过 `initcode.S` 塞进内存。

你当前这份仓库不是这样。它的主线是：

`main -> userinit -> scheduler -> forkret -> fsinit -> kexec("/init") -> prepare_return -> userret -> /init`

也就是说：

- `userinit()` 只创建一个空的初始进程
- 第一次真正运行它时，会进入 `forkret()`
- `forkret()` 里执行 `kexec("/init", ...)`
- `/init` 才是第一个实际运行的用户程序

### 这一阶段必须搞懂

- `forkret()` 的职责
- `kexec()` 的输入和返回值
- ELF 是如何被装进当前进程地址空间的
- 为什么 `argc` 在 `a0`，`argv` 在 `a1`

### 建议输出

- 画出首进程启动链
- 解释 `p->context.ra = forkret` 的意义

### 自测问题

- `kexec()` 改变的是“当前进程”还是“创建新进程”？
- `/init` 启动后为什么还能再 `exec("sh")`？
- 这份代码里 `fsinit(ROOTDEV)` 为什么不能放在 `main()` 里做？

---

## 6. 学系统调用接口层

### 目标

理解用户态是怎么向内核请求服务的。

### 必读文件

- `user/user.h`
- `user/usys.pl`
- `user/usys.S`
- `kernel/syscall.h`
- `kernel/syscall.c`
- `kernel/sysproc.c`
- `kernel/sysfile.c`

### 这一阶段必须搞懂

- 用户态封装函数只是 API 外壳
- `usys.S` 负责发起 `ecall`
- `syscall.c` 负责按 syscall number 分发
- 真正业务逻辑通常在 `proc.c`、`file.c`、`fs.c`、`exec.c` 里

### 建议输出

- 选一个 syscall，完整追踪一遍
- 推荐先追：`fork`、`exec`、`write`、`open`

### 自测问题

- `sys_exec()` 和 `kexec()` 有什么关系？
- 为什么 `syscall.c` 本身不承担太多业务逻辑？
- 用户态 `printf` 和内核态 `printf` 是不是同一个实现？

---

## 7. 学用户空间最小生态

### 目标

理解“内核 + init + shell + 命令”为什么已经构成一个最小 OS。

### 必读文件

- `user/init.c`
- `user/sh.c`
- `user/ls.c`
- `user/cat.c`
- `user/echo.c`
- `user/user.h`
- `user/ulib.c`
- `user/printf.c`
- `user/umalloc.c`

### 这一阶段必须搞懂

- `/init` 是第一个用户进程
- `sh` 是交互入口
- 普通命令程序只是 syscall 的使用者
- 用户态也有自己的简化运行库

### 建议输出

- 画出 `/init -> sh -> 命令程序` 的关系图
- 理解为什么 shell 不在内核里实现

### 自测问题

- `sh` 为什么需要 `fork + exec + wait`？
- 为什么 `ls`、`cat` 这些程序可以很小？

---

## 8. 再学文件系统主线

### 目标

理解 OS 怎么管理持久化数据、路径名和打开文件对象。

### 必读文件

- `kernel/bio.c`
- `kernel/buf.h`
- `kernel/log.c`
- `kernel/fs.h`
- `kernel/fs.c`
- `kernel/file.h`
- `kernel/file.c`
- `kernel/sysfile.c`
- `kernel/stat.h`
- `kernel/fcntl.h`

### 建议阅读顺序

1. `bio.c`：块缓存
2. `log.c`：日志
3. `fs.c`：inode、目录、路径名
4. `file.c`：打开文件对象层
5. `sysfile.c`：文件类系统调用入口

### 这一阶段必须搞懂

- 磁盘块、buffer、inode、file、fd 不是一回事
- `file` 是“内核打开文件对象”
- `fd` 是“进程视角的整数句柄”
- 日志层保证文件系统操作的一致性

### 建议输出

- 画出 `open/read/write/close` 路径
- 画出 `fd -> struct file -> inode -> disk block` 的关系

### 自测问题

- 为什么 xv6 需要 buffer cache？
- 为什么文件系统操作要先经过日志层？
- `namei()` 在整个文件系统里处于什么地位？

---

## 9. 最后学设备和驱动

### 目标

理解 OS 怎么和真实硬件或虚拟硬件交互。

### 必读文件

- `kernel/console.c`
- `kernel/uart.c`
- `kernel/virtio_disk.c`
- `kernel/virtio.h`
- `kernel/plic.c`

### 这一阶段必须搞懂

- `uart` 提供字符输入输出
- `console` 在 `uart` 之上做终端抽象
- `virtio_disk` 是块设备驱动
- `plic` 负责把设备中断送给 CPU

### 建议输出

- 跟踪一次字符输入到 shell 的链路
- 跟踪一次磁盘 I/O 请求和中断完成链路

### 自测问题

- `console` 和 `uart` 的职责边界是什么？
- 磁盘中断是怎样回到内核代码里的？
- 为什么设备管理和 trap 机制天然耦合？

---

## 10. 回到“一个 OS 的六大管理功能”

当前你已经按运行链路学完了，再回头做功能重组：

- 总体框架：`entry/start/main/kernel.ld/memlayout`
- 处理机管理：`trap/swtch/scheduler/plic`
- 内存管理：`kalloc/vm/exec 地址空间部分`
- 进程管理：`proc/fork/exit/wait/sleep/wakeup`
- 文件管理：`bio/log/fs/file/sysfile`
- 设备管理：`uart/console/virtio/plic`
- 接口管理：`usys/syscall/sysproc/sysfile`

这一步的目标不是再读一遍代码，而是把你已经理解的运行主线，重新翻译成教材里的术语。

---

## 建议的四周节奏

### 第 1 周：让内核“活起来”

- 第 1 天：`entry.S`, `start.c`, `main.c`
- 第 2 天：`kernel.ld`, `memlayout.h`
- 第 3-4 天：`kalloc.c`, `vm.c`
- 第 5-6 天：`trap.c`, `kernelvec.S`, `trampoline.S`
- 第 7 天：把启动链和 trap 链完整默写一遍

### 第 2 周：让进程和用户态“活起来”

- 第 1-2 天：`proc.h`, `proc.c`
- 第 3 天：`swtch.S`
- 第 4 天：`forkret`, `userinit`, `scheduler`
- 第 5-6 天：`exec.c`, `user/init.c`
- 第 7 天：画出首进程启动链

### 第 3 周：让接口和用户空间“活起来”

- 第 1-2 天：`user.h`, `usys.pl`, `usys.S`, `syscall.c`
- 第 3-4 天：`sysproc.c`, `sysfile.c`
- 第 5-7 天：`init.c`, `sh.c`, `ls.c`, `cat.c`

### 第 4 周：让文件和设备“活起来”

- 第 1-3 天：`bio.c`, `log.c`, `fs.c`, `file.c`
- 第 4-5 天：`console.c`, `uart.c`, `virtio_disk.c`, `plic.c`
- 第 6-7 天：按“六大管理功能”复盘全部代码

---

## 每阶段统一学习方法

每读一个阶段，都只做四件事：

1. 先抓主线函数，不追所有细节
2. 画调用链
3. 画关键数据结构关系
4. 回答“删掉这一层后，系统会死在哪里”

推荐你每个阶段都写三种笔记：

- 一页“流程图”
- 一页“结构体字段解释”
- 一页“我现在能回答的问题”

---

## 建议你优先跟踪的 8 条主线

1. 启动主线：`entry -> start -> main`
2. 内存主线：`kinit -> kalloc -> kvminit`
3. trap 主线：`ecall -> usertrap -> syscall -> prepare_return -> userret`
4. 调度主线：`scheduler -> swtch -> forkret`
5. 首进程主线：`userinit -> forkret -> kexec("/init")`
6. shell 主线：`/init -> exec("sh")`
7. 文件主线：`open/read/write -> sysfile -> file/fs`
8. 设备主线：`uart/virtio interrupt -> trap -> driver`

如果这 8 条主线能讲通，整个 xv6 基本就立起来了。

---

## 最后的判断标准

学完以后，你至少应该能独立回答这些问题：

- xv6 是怎样从上电走到 shell 提示符的？
- 进程为什么既需要 `context`，也需要 `trapframe`？
- `fork` 和 `exec` 为什么是两个不同维度的操作？
- 为什么系统调用、中断、异常能共用 trap 机制？
- 文件系统为什么要分成 `fd / file / inode / block` 几层？
- 驱动和中断控制器为什么不能脱离 CPU trap 机制单独理解？

如果这些问题能讲明白，你就不是“会看 xv6 文件名”，而是已经开始真正理解一个 OS 了。
