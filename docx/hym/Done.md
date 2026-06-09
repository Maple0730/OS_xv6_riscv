## 接口、设备与软件交互 — 已完成工作

> 负责人：侯奕明
> 更新日期：2026-06-09

---

### 一、个人开展计划制定

已完成完整的个人开展计划文档，明确了分工定位、分析主线和三阶段时间规划。

**对应文件**：`houyiming1.md`（仓库根目录）

#### 分工定位

我（侯奕明）负责 **"接口、设备与软件交互"**，覆盖 A 题中的：

| A题模块 | 与我相关的部分 | 重要程度 |
|---------|---------------|:------:|
| **2. 中断与异常处理** | 几乎全部（ecall系统调用、时钟中断、设备中断、缺页异常） | ★★★★★ |
| **6. 用户程序加载与执行** | 系统调用封装库、shell与内核接口 | ★★★☆☆ |
| **1. 系统启动** | start.c 中 M→S 切换、timerinit 与后续 trap 初始化的衔接 | ★★☆☆☆ |

#### 五条分析主线已规划

1. **一次系统调用的完整生命周期** 🔑 — 最重要
2. **中断处理的两条路径**（用户态被中断 vs 内核态被中断）
3. **控制台 I/O 的完整链路**
4. **设备中断框架**（PLIC + 设备驱动）
5. **Shell 与系统调用的接口关系**

#### 三层文件梯队已划分

- **第一梯队（吃透）**：`trap.c`, `syscall.c`, `trampoline.S`
- **第二梯队（深入）**：`console.c`, `uart.c`, `plic.c`, `kernelvec.S`
- **第三梯队（把握）**：`sysproc.c`, `sysfile.c`, `pipe.c`, `start.c`, `sh.c`, `usys.pl`

---

### 二、xv6-riscv 运行 C 程序原理分析

已完成对 xv6-riscv 如何运行 C 语言程序的完整分析。

**对应文件**：`houyiming6.9.md`（仓库根目录）

#### 分析内容

1. **C 程序编译机制**
   - 交叉编译器 `riscv64-unknown-elf-gcc`
   - `-ffreestanding`、`-nostdlib`、`-march=rv64gc` 等关键编译选项
   - ELF 格式可执行文件的生成与打包（`mkfs` → `fs.img`）

2. **系统调用机制**
   - `user/usys.pl` → 汇编 stub（`li a7, SYS_write; ecall; ret`）
   - 内核 `syscalls[]` 表分发
   - 完整的用户态→内核态→用户态路径

3. **exec 系统调用分析**
   - 内核层面：`kernel/exec.c` 的 `kexec()` 函数
     - 读取 ELF 头（验证魔数 `\x7FELF`）
     - 遍历 Program Header 加载段
     - 分配用户栈、拷贝参数
     - 设置 `epc = elf.entry`
   - Shell 层面：`user/sh.c` 的 `fork + exec` 机制
   - `user/ulib.c` 中 `start()` 函数调用 `main()` 的机制

4. **一条命令的完整执行路径**（以 `echo hello` 为例）
   ```
   sh.c: gets() → parsecmd() → fork() → exec("echo", ...)
     → ecall 陷入内核 → kexec() 加载 ELF
     → 返回用户态 → start() → main() → 执行 → exit(0)
   ```

---

### 三、22 个系统调用覆盖表整理

已完成全部 22 个系统调用（原 21 个 + 新增 `ps`）的编号、功能和分析重点整理。

| 编号 | 系统调用 | 功能 | 我的分析重点 |
|:--:|---------|------|------------|
| 1 | SYS_fork | 创建子进程 | 无参调用，返回值在a0 |
| 2 | SYS_exit | 退出进程 | 退出状态传递 |
| 3 | SYS_wait | 等待子进程 | 状态指针地址传递 |
| 4 | SYS_pipe | 创建管道 | fdarray 在用户/内核间传递 |
| 5 | SYS_read | 读文件 | 控制台读的睡眠/唤醒 |
| 6 | SYS_kill | 终止进程 | pid 参数 |
| 7 | SYS_exec | 执行程序 | 路径字符串和 argv 传递 |
| 8 | SYS_fstat | 获取文件状态 | stat 结构体内核→用户拷贝 |
| 9 | SYS_chdir | 切换目录 | 路径字符串 |
| 10 | SYS_dup | 复制fd | 重定向实现基础 |
| 11 | SYS_getpid | 获取进程ID | 最简单 syscall 示例 |
| 12 | SYS_sbrk | 扩展内存 | lazy allocation |
| 13 | SYS_pause | 睡眠 | ticks 睡眠/唤醒 |
| 14 | SYS_uptime | 获取运行时间 | ticks 计数器 |
| 15 | SYS_open | 打开文件 | 路径+模式 |
| 16 | SYS_write | 写文件 | 最常用 syscall |
| 17 | SYS_mknod | 创建设备文件 | major/minor 设备号 |
| 18 | SYS_unlink | 删除文件 | 路径解析 |
| 19 | SYS_link | 创建硬链接 | 链接计数 |
| 20 | SYS_mkdir | 创建目录 | . 和 .. 条目 |
| 21 | SYS_close | 关闭文件 | fd 回收 |
| 22 | **SYS_ps** | **打印进程列表** | **🆕 本次新增，复用 procdump()** |

---

### 四、新增 `ps` 系统调用 🆕

已成功实现 `ps` 系统调用，让用户程序能够从用户态打印所有进程的状态信息。

#### 修改文件清单

| 文件 | 修改内容 |
|------|---------|
| `kernel/syscall.h` | 新增 `#define SYS_ps 22` — 分配系统调用号 22 |
| `kernel/syscall.c` | 新增 `extern uint64 sys_ps(void);` 声明，在 `syscalls[]` 表中注册 `[SYS_ps] sys_ps` |
| `kernel/sysproc.c` | 新增 `sys_ps()` 函数（100-104行），直接复用内核已有的 `procdump()` |
| `user/usys.pl` | 新增 `entry("ps");` — 自动生成用户态汇编 stub |
| `user/user.h` | 新增 `int ps(void);` — 用户态函数声明 |
| `user/ps.c` | **新建文件** — 用户程序，调用 `ps()` 后 `exit(0)` |

#### 实现原理

```
用户程序 ps.c: ps()
  → user/usys.pl 生成的 stub: li a7, SYS_ps; ecall; ret
  → trap.c:usertrap() 捕获 ecall
  → syscall.c:syscall() 查 syscalls[] 表，分发到 sys_ps()
  → sysproc.c:sys_ps() → procdump()
  → 遍历 proc[NPROC]，打印每个非 UNUSED 进程的 pid、state、name
```

#### 设计要点

- **零新增内核逻辑**：`sys_ps()` 直接复用 `proc.c` 中已有的 `procdump()`，该函数原本只能通过 Ctrl-P 在内核态触发，现在通过系统调用暴露给用户态
- **procdump() 输出内容**：每个进程的 pid、状态（unused/used/sleep/runble/run/zombie）、进程名
- **系统调用号为 22**：紧接原 21 个系统调用之后，保持编号连续

---

### 五、与组员的接口配合梳理

已明确与涂凡琛（进程管理/调度）、韩硕（内存管理/文件系统）之间的接口关系：

| 接口点 | 对方负责 | 我的部分 |
|-------|---------|---------|
| `trap.c:85` — `yield()` | 涂凡琛（调度器） | 时钟中断触发 yield 的路径 |
| `syscall.c` 中 fork/exit/wait/kill | 涂凡琛（进程管理） | 系统调用分发机制和参数传递 |
| `trap.c:71-73` — vmfault() | 韩硕（内存管理） | 缺页异常的捕获和分发 |
| `sys_sbrk()` | 韩硕（内存管理） | 系统调用的参数传递 |
| `sys_read/sys_write` 的文件操作 | 韩硕（文件系统） | 系统调用层到文件层的调用链 |

---

### 小结

> 已完成个人分工定位、22个系统调用覆盖表、C程序运行原理分析、exec系统调用分析，以及 `ps` 系统调用的完整实现（用户态→内核态全链路），正在深入 trap.c/syscall.c 的核心代码精读和时序图绘制。
