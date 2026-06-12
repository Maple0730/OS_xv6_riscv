# 侯奕明 — 操作系统课程设计 A 题个人开展计划

> 生成时间：2026年6月8日  
> 小组成员：涂凡琛、侯奕明、韩硕  
> 选题：A 题 · OS 内核实现  
> 基础平台：xv6-riscv

---

## 一、我的分工定位

根据小组分工，我（侯奕明）负责 **"接口、设备与软件交互"**，具体覆盖 A 题六大模块中的：

| A题模块 | 与我相关的部分 | 重要程度 |
|---------|---------------|:------:|
| **2. 中断与异常处理** | 几乎全部（ecall系统调用、时钟中断、设备中断、缺页异常） | ★★★★★ |
| **6. 用户程序加载与执行** | 系统调用封装库、shell与内核接口 | ★★★☆☆ |
| **1. 系统启动** | start.c 中 M→S 切换、timerinit 与后续 trap 初始化的衔接 | ★★☆☆☆ |

简单说：**我负责的是用户程序 ↔ 内核之间的胶水层**——系统调用如何从用户态进入内核、中断如何被捕获和分发、设备（UART/控制台）如何与软件交互。

---

## 二、A 题要求与我负责部分的对应关系

### 2. 中断与异常处理（A 题第 2 模块）⭐ 核心

| 技术要求 | xv6 当前实现 | 我的分析任务 |
|---------|-------------|------------|
| 时钟中断频率稳定（约100Hz） | `clockintr()` 中 `w_stimecmp(r_time() + 1000000)` ≈ 每10ms一次 | 验证频率、画中断路径 |
| 键盘中断能正确读取扫描码并转换为ASCII | `uartgetc()` 读 RHR → `consoleintr()` 处理特殊键（退格/Ctrl-U/Ctrl-D） | 分析 UART→控制台 全链路 |
| 系统调用支持：打印、进程创建、进程退出、文件读写 | `syscalls[]` 表已注册21个系统调用 | 逐项验证、整理调用表 |
| 实现系统调用接口（ecall） | `usys.pl` → `ecall` → `usertrap()` → `syscall()` 完整链路 | **重点分析：画全链路时序图** |
| 实现缺页异常处理 | `usertrap()` 中 scause==13/15 → `vmfault()`（lazy allocation） | 分析缺页处理路径 |

### 6. 用户程序加载与执行（A 题第 6 模块）

| 技术要求 | xv6 当前实现 | 我的分析任务 |
|---------|-------------|------------|
| 系统调用封装库（简化 libc） | `user/usys.pl` 生成 stub, `user/user.h` 声明, `user/ulib.c` 实现 | 分析 stub 生成机制 |
| shell 支持 ls/cat/echo | `user/sh.c` 已有 shell，`ls.c`/`cat.c`/`echo.c` 已有 | 分析 shell 如何调用系统调用 |
| shell 支持 ps | ✗ 缺失 | **新增 sys_ps 系统调用** |
| shell 支持 kill | 已有 `kill.c` | 验证即可 |
| shell 支持管道 | `user/sh.c` + `kernel/pipe.c` | **重点分析：管道实现机制** |

---

## 三、我需要掌握的核心文件

### 第一梯队（必须吃透）

```
kernel/trap.c          — trap 总控中心：usertrap/kerneltrap/devintr/clockintr
kernel/syscall.c       — 系统调用分发中心
kernel/trampoline.S    — 用户态↔内核态切换的汇编桥梁
```

### 第二梯队（深入理解）

```
kernel/console.c       — 控制台输入输出（与 shell 的接口）
kernel/uart.c          — 16550a UART 驱动（设备中断的来源）
kernel/plic.c          — PLIC 中断控制器
kernel/kernelvec.S     — 内核态 trap 汇编入口
```

### 第三梯队（整体把握）

```
kernel/sysproc.c       — 进程相关系统调用实现
kernel/sysfile.c       — 文件相关系统调用实现
kernel/pipe.c          — 管道实现（shell 管道依赖）
kernel/start.c         — timerinit() 时钟中断的源头
kernel/riscv.h         — CSR 寄存器、页表宏定义
user/sh.c              — shell 如何调用系统调用
user/usys.pl           — 用户态系统调用 stub 生成脚本
user/user.h            — 用户态库函数声明
```

---

## 四、五条分析主线

### 主线 1：一次系统调用的完整生命周期 🔑

这是我个人汇报中**最重要**的内容。

```
用户程序调用 write()
  → user/usys.pl 生成的汇编 stub: li a7, SYS_write; ecall; ret
  → CPU 硬件：ecall 触发 trap，scause=8
  → stvec 指向 TRAMPOLINE 页中的 uservec
  → trampoline.S:uservec：保存用户寄存器到 trapframe，切内核页表，跳转 usertrap()
  → trap.c:usertrap()：识别 scause==8→系统调用，epc+=4，intr_on()，调用 syscall()
  → syscall.c:syscall()：从 trapframe->a7 取系统调用号，查 syscalls[] 表，调用 sys_write()
  → sysfile.c:sys_write()：argfd/argaddr/argint 取参数，调用 filewrite()
  → 返回后 trap.c:prepare_return()：关中断，设 stvec=uservec，填 trapframe，设 sepc，sret 回用户态
  → trampoline.S:userret：恢复用户寄存器，切回用户页表，sret 回用户态
```

**关键代码位置（需要能逐行解释）**：

| 文件:行号 | 关键代码 | 解释要点 |
|-----------|---------|---------|
| `trampoline.S:22-98` | uservec | 为什么trampoline要同时映射在内核和用户页表？ |
| `trap.c:54` | `if(r_scause()==8)` | scause=8是谁写的？为什么是8？ |
| `trap.c:62` | `epc += 4` | 为什么是+4？（RISC-V ecall指令是4字节） |
| `trap.c:66` | `intr_on()` | 为什么在这里开中断而不是更早？ |
| `syscall.c:140` | `num = p->trapframe->a7` | a7的值是谁放进去的？（usys.pl的stub） |
| `syscall.c:144` | `p->trapframe->a0 = syscalls[num]()` | 返回值为什么写入a0？ |
| `trap.c:112` | `w_stvec(trampoline_uservec)` | 为什么这里切回uservec？ |
| `trap.c:124-128` | `SPP=0, SPIE=1` | SPP和SPIE分别控制什么？ |

---

### 主线 2：中断处理的两条路径

#### 路径 A：用户态被中断

```
用户进程运行中 → 时钟/设备中断
  → uservec (trampoline.S) → usertrap()
  → devintr() 判断中断类型：
     - scause=0x8000000000000005 → timer interrupt → clockintr() → return 2
     - scause=0x8000000000000009 → external interrupt → plic_claim()
       → irq==UART0_IRQ → uartintr() → consoleintr()
       → irq==VIRTIO0_IRQ → virtio_disk_intr()
  → which_dev==2 则 yield()  ← 时间片轮转的关键触发点！
  → prepare_return() → userret → 回用户态
```

#### 路径 B：内核态被中断

```
内核代码运行中 → 时钟/设备中断
  → kernelvec (kernelvec.S) → kerneltrap()
  → devintr() 同样处理
  → which_dev==2 则 yield()
  → 恢复 sepc/sstatus，sret 回内核继续执行
```

**关键点**：
- `trap.c:85` — `if (which_dev == 2) yield()` — 这是**时间片调度的触发点**
- `trap.c:187-220` — devintr() 的完整分流逻辑
- `trap.c:167-180` — clockintr()：ticks++、wakeup、预约下一次时钟中断
- `trap.c:29 vs trap.c:112` — stvec 在内核态 vs 回用户态前的切换（kernelvec ↔ uservec）

---

### 主线 3：控制台 I/O 的完整链路

```
用户态 read(0, buf, n)
  → sys_read() → fileread() → devsw[CONSOLE].read = consoleread()
  → consoleread()：在 cons.lock 保护下，等待 cons.r != cons.w，逐字符拷贝

键盘输入 → UART 硬件产生中断
  → plic_claim() → UART0_IRQ → uartintr()
  → uartgetc() 读 RHR 寄存器 → consoleintr(c)
  → consoleintr()：处理退格/Ctrl-U/Ctrl-D，填环形缓冲区 cons.buf，wakeup(&cons.r)

用户态 write(1, buf, n)
  → sys_write() → filewrite() → devsw[CONSOLE].write = consolewrite()
  → consolewrite() → uartwrite() → 逐字符写 THR，等待 UART 发送完成中断
```

**关键数据结构**：
- `cons` 结构体（`console.c:47-56`）：环形缓冲区，r/w/e 三指针设计
- `consoleintr()` 的行编辑功能：退格(Ctrl-H)、删行(Ctrl-U)、EOF(Ctrl-D)、打印进程(Ctrl-P)

---

### 主线 4：设备中断框架（PLIC + 设备驱动）

```
PLIC 初始化：
  main.c → plicinit()：设 UART(irq=10) 和 virtio(irq=1) 的中断优先级为 1
  main.c → plicinithart()：使能当前 hart 的 S-mode 中断，设优先级阈值=0

中断到来时：
  devintr() 识别 scause=0x8000000000000009（supervisor external interrupt）
  → plic_claim() 读当前 hart 的 SCLAIM 寄存器，得到 irq 号
  → 根据 irq 分发到具体设备处理函数
  → plic_complete(irq) 写回 SCLAIM，告知 PLIC 处理完毕
```

---

### 主线 5：Shell 与系统调用的接口关系

| shell 功能 | 涉及的系统调用 | 分析要点 |
|-----------|---------------|---------|
| 执行命令 `ls` | fork + exec + wait | exec如何加载ELF、如何传参 |
| 重定向 `ls > out` | close + open + dup | dup如何替换fd、open返回什么 |
| 管道 `ls \| wc` | pipe + fork×2 + close + dup | pipe的读写同步机制 |
| 后台 `cmd &` | fork（不等wait） | 父子进程并发 |
| 顺序 `a; b` | fork + wait + 再fork | 进程等待链 |

**管道实现要点**（`kernel/pipe.c`）：
- `struct pipe`：512字节环形缓冲区 + 读写指针 + 打开状态
- 写满时写者sleep等读者读
- 读空时读者sleep等写者写
- 一端关闭时唤醒另一端

---

## 五、x系统调用覆盖表（21 个）

| 编号 | 系统调用 | 功能 | 我的分析重点 |
|:--:|---------|------|------------|
| 1 | SYS_fork | 创建子进程 | 参数如何传递（无参，返回值在a0） |
| 2 | SYS_exit | 退出进程 | 退出状态如何传递给父进程 |
| 3 | SYS_wait | 等待子进程 | 返回状态指针的地址传递 |
| 4 | SYS_pipe | 创建管道 | **重点：fdarray如何在用户/内核间传递** |
| 5 | SYS_read | 读文件 | **重点：控制台读的睡眠/唤醒机制** |
| 6 | SYS_kill | 终止进程 | pid参数传递 |
| 7 | SYS_exec | 执行程序 | **重点：路径字符串和argv数组的传递** |
| 8 | SYS_fstat | 获取文件状态 | stat结构体从内核拷到用户空间 |
| 9 | SYS_chdir | 切换目录 | 路径字符串传递 |
| 10 | SYS_dup | 复制fd | **重点：重定向的实现基础** |
| 11 | SYS_getpid | 获取进程ID | 最简单的syscall示例 |
| 12 | SYS_sbrk | 扩展内存 | lazy allocation 的实现 |
| 13 | SYS_pause | 睡眠 | ticks 的睡眠/唤醒 |
| 14 | SYS_uptime | 获取运行时间 | ticks 计数器的使用 |
| 15 | SYS_open | 打开文件 | 路径+模式参数 |
| 16 | SYS_write | 写文件 | **重点：最常用的syscall之一** |
| 17 | SYS_mknod | 创建设备文件 | major/minor 设备号 |
| 18 | SYS_unlink | 删除文件 | 路径解析 |
| 19 | SYS_link | 创建硬链接 | 链接计数 |
| 20 | SYS_mkdir | 创建目录 | 目录的 . 和 .. 条目 |
| 21 | SYS_close | 关闭文件 | fd回收 |

---

## 六、验证实验计划

### 实验1：验证时钟中断频率

写一个用户程序，调用 `uptime()` 系统调用，pause 一段时间，再次调用，计算 tick 差值，验证是否约 100Hz。

### 实验2：验证键盘中断

在 shell 中测试各种输入场景：普通字符、退格、Ctrl-U（删行）、Ctrl-D（EOF）、Ctrl-P（打印进程列表），观察行为是否符合 `consoleintr()` 的设计。

### 实验3：验证缺页异常（lazy allocation）

写用户程序，用 `sbrklazy()` 申请大量内存但不立即使用，然后逐步访问新页，观察是否每次访问新页时触发 `vmfault()` 补页。

### 实验4：验证系统调用全链路

用 GDB 打断点在 `usertrap()`、`syscall()`、`prepare_return()`，单步跟踪一次 `write(1, "hello", 5)` 的完整生命周期。

---

## 七、扩展计划

在完成基础分析和验证后，我计划做以下扩展：

### 1. 新增 `ps` 系统调用（展示进程列表）

当前 xv6 只能在 Ctrl-P 时在内核 printf 进程列表，需要新增一个系统调用让用户程序能获取进程信息：

```c
// 用户程序 ps.c
#include "kernel/types.h"
#include "user/user.h"

int main() {
    ps();  // 新增系统调用，打印所有进程状态
    exit(0);
}
```

需要在 `kernel/sysproc.c` 中新增 `sys_ps()`，遍历 `proc[]` 数组并输出进程信息。

### 2. 写管道测试程序

```c
// pipetest.c
int main() {
    int p[2];
    pipe(p);
    if (fork() == 0) {
        close(p[0]);
        write(p[1], "hello pipe", 10);
        close(p[1]);
        exit(0);
    } else {
        close(p[1]);
        char buf[32];
        int n = read(p[0], buf, sizeof(buf));
        printf("parent read: %.*s\n", n, buf);
        close(p[0]);
        wait(0);
    }
    exit(0);
}
```

### 3. 写中断测试程序

展示 `read()` 阻塞等待、键盘中断唤醒的完整过程。

---

## 八、与组员的接口配合

| 接口点 | 对方负责 | 我的部分 |
|-------|---------|---------|
| `trap.c:85` — `yield()` | 涂凡琛（调度器） | 时钟中断触发yield的路径 |
| `syscall.c` 中 fork/exit/wait/kill | 涂凡琛（进程管理） | 系统调用分发机制和参数传递 |
| `trap.c:71-73` — vmfault() | 韩硕（内存管理） | 缺页异常的捕获和分发 |
| `sys_sbrk()` | 韩硕（内存管理） | 系统调用的参数传递 |
| `sys_read/sys_write` 的文件操作 | 韩硕（文件系统） | 系统调用层到文件层的调用链 |

---

## 九、三阶段时间规划

### 第 14-15 周（当前 ~ 6月中旬）：代码精读 + 画图

- [ ] 画出 ecall → syscall → sret 完整时序图（标注特权级切换）
- [ ] 画出 时钟中断 → devintr → clockintr → yield 的数据流图
- [ ] 画出 键盘 → UART中断 → uartintr → consoleintr → shell读 的数据流图
- [ ] 画出 shell 执行 `ls | wc` 的 fork/pipe/dup/close 调用序列
- [ ] 逐文件精读并写注释（trap.c, syscall.c, console.c, uart.c）
- [ ] 验证所有 21 个系统调用可用

### 第 15-16 周（6月中下旬）：功能实现 + 测试

- [ ] 新增 `sys_ps` 系统调用
- [ ] 写 `pipetest.c` 用户程序验证管道
- [ ] 写 `intrtest.c` 用户程序验证中断/输入
- [ ] 完善 shell 的 ps/kill 命令支持

### 第 16-17 周（6月下旬 ~ 汇报）：文档 + 展示

- [ ] 完成"中断与异常处理"模块文档（对应A题第2项）
- [ ] 完成"用户程序加载与执行"中我负责的部分
- [ ] 准备答辩 PPT（系统调用/中断/设备部分）
- [ ] 准备现场 demo（演示系统调用、中断处理、shell操作）

---

## 十、当前最该做的三件事

1. **吃透 `trap.c:54-94`（usertrap函数）** — 能对着代码讲出"一个 `write()` 调用从这里走到哪里、每个寄存器怎么变"
2. **在 xv6 中跑 `make qemu`，在 shell 中逐条执行命令** — 执行 ls、echo、cat、管道、重定向，记录每个命令涉及哪些系统调用
3. **开始写"系统调用全链路分析"文档** — 这是我个人汇报最重要的内容，也是我负责部分的核心价值

---

## 参考资料

- xv6-riscv 源码：`/home/houyiming/桌面/OS_xv6_riscv/kernel/`
- 大赛测例参考：`/home/houyiming/桌面/OS_xv6_riscv/testsuits-for-oskernel-on-site-final-2025/`
- 课程设计题目：`/home/houyiming/桌面/OS_xv6_riscv/《操作系统课程设计》项目制题目2026.pdf`
- 小组进度报告：`/home/houyiming/桌面/OS_xv6_riscv/docx/tex/A题进度报告.tex`
- 学习笔记：`/home/houyiming/桌面/OS_xv6_riscv/docx/note.md`
