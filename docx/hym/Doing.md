## 接口、设备与软件交互

> 负责人：侯奕明
> 更新日期：2026-06-09

---

### 一、系统调用全链路分析 🔑

正在深入分析 **一次系统调用的完整生命周期**，这是我负责部分最核心的内容。

#### 当前进展

1. **已完成整体链路梳理**（见 [Done.md](./Done.md)），正在逐函数精读：
   - `trampoline.S:uservec` — 用户态寄存器保存、页表切换
   - `trap.c:usertrap()` — trap 分发中心（54-94行）
   - `trap.c:prepare_return()` — 返回用户态前的准备工作
   - `syscall.c:syscall()` — 系统调用号查表分发
   - `user/usys.pl` — 用户态 stub 自动生成机制

2. **正在画的图**：
   - ecall → uservec → usertrap → syscall → prepare_return → userret → sret 完整时序图
   - 标注每个阶段的特权级切换（U-mode ↔ S-mode）和关键寄存器变化

3. **需要能逐行解释的关键代码位置**：

   | 文件:行号 | 关键代码 | 要点 |
   |-----------|---------|------|
   | `trampoline.S:22-98` | uservec | 为什么trampoline要同时映射在内核和用户页表？ |
   | `trap.c:54` | `if(r_scause()==8)` | scause=8 是谁写的？ |
   | `trap.c:62` | `epc += 4` | 为什么+4？（RISC-V ecall 指令是4字节） |
   | `trap.c:66` | `intr_on()` | 为什么在这里开中断？ |
   | `syscall.c:140` | `num = p->trapframe->a7` | a7 谁放的？（usys.pl stub） |
   | `syscall.c:144` | `p->trapframe->a0 = ...` | 返回值为什么写 a0？ |
   | `trap.c:112` | `w_stvec(trampoline_uservec)` | 为什么切回 uservec？ |
   | `trap.c:124-128` | `SPP=0, SPIE=1` | SPP/SPIE 控制什么？ |

---

### 二、中断处理两条路径分析

正在分析中断在内核态 vs 用户态两种场景下的不同处理路径：

#### 路径 A：用户态被中断
```
用户进程运行中
  → 时钟/设备中断
  → uservec (trampoline.S)
  → usertrap()
  → devintr() 判断中断类型
  → 如果是时钟中断 → clockintr() → yield()  ← 时间片轮转触发点！
  → prepare_return() → userret → sret
```

#### 路径 B：内核态被中断
```
内核代码运行中
  → 中断
  → kernelvec (kernelvec.S)
  → kerneltrap()
  → devintr() 同样处理
  → yield() 或恢复执行
  → sret 回内核继续
```

**正在分析的关键点**：
- `trap.c:85` — `if (which_dev == 2) yield()` — 时间片调度触发点
- `trap.c:187-220` — devintr() 的完整分流逻辑
- `trap.c:167-180` — clockintr()：ticks++、wakeup、预约下一次中断
- `trap.c:29 vs trap.c:112` — stvec 在内核态 vs 回用户态前的切换

---

### 三、控制台 I/O 链路分析

正在追踪从键盘输入到 shell 读取的完整数据流：

```
键盘 → UART 硬件中断
  → plic_claim() → UART0_IRQ → uartintr()
  → uartgetc() 读 RHR 寄存器
  → consoleintr(c) 处理特殊键
  → 填环形缓冲区 cons.buf
  → wakeup(&cons.r)
  → consoleread() 被唤醒 → 逐字符拷贝到用户态
```

**关键数据结构**：`cons` 结构体（`console.c:47-56`）的环形缓冲区设计（r/w/e 三指针）

---

### 四、下一步

- [ ] 完成时序图的绘制
- [ ] 用 GDB 单步跟踪一次 `write(1, "hello", 5)` 的完整生命周期
- [ ] 开始写"系统调用全链路分析"文档正文
