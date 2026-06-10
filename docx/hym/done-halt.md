## halt — 新增 halt 关机系统调用 ✅

> 负责人：侯奕明
> 完成日期：2026-06-10

---

### 一、概述

新增 `halt` 系统调用（编号 23），允许用户在 shell 中输入 `halt` 命令来关机退出 QEMU。通过内核态的 SBI `ecall` 指令通知 OpenSBI 关机，QEMU 收到后退出。

`halt` 实现为 **shell 内建命令**（类似 `cd`），直接在 shell 进程中调用 `halt()` 系统调用，不需要 fork/exec 外部程序。同时也保留了 `user/halt.c` 作为独立的用户程序，可通过 `exec` 调用。

**涉及文件**：9 个文件（1 新建 + 8 修改）

---

### 二、修改文件清单

| 文件 | 修改内容 | 具体操作 |
|------|---------|---------|
| `kernel/riscv.h` | 新增 `sbi_shutdown()` | 内联汇编：`li a7, 0x08; li a6, 0; ecall` 调用 SBI legacy shutdown |
| `kernel/start.c` | 修复 `medeleg` | `w_medeleg(0xffff)` → `w_medeleg(0xffff & ~(1L << 9))`，不清除 bit 9（S-mode ecall），确保 ecall 能到达 OpenSBI |
| `kernel/syscall.h` | 新增系统调用号 | `#define SYS_halt 23` |
| `kernel/syscall.c` | 注册系统调用 | 新增 `extern uint64 sys_halt(void);` 声明 + `syscalls[]` 表中注册 `[SYS_halt] sys_halt` |
| `kernel/sysproc.c` | 新增 `sys_halt()` 函数 | 直接调用 `sbi_shutdown()` |
| `user/usys.pl` | 生成用户态 stub | 新增 `entry("halt");` |
| `user/user.h` | 新增函数声明 | `int halt(void);` |
| `user/sh.c` | **shell 内建 halt** | 在 `main()` 中增加 `halt` 内建命令检测，直接调用 `halt()`，不 fork |
| `user/halt.c` | **新建**用户程序 | 打印 "shutting down..." 后调用 `halt()`（保留作为独立可执行文件） |
| `Makefile` | 加入 UPROGS | 新增 `$(BU)/_halt\` |

---

### 三、实现原理

#### 两条调用路径

**路径一：shell 内建命令**（类似 `cd`，不 fork）
```
shell 中输入 "halt"
  ──▶ sh.c: main() 内建检测 → printf("shutting down...") → halt()
        ──▶ usys.S: li a7, SYS_halt; ecall; ret     [用户态 ecall → S-mode]
              ──▶ trap.c: usertrap() → syscall()
                    ──▶ syscall.c: syscalls[23] → sys_halt()
                          ──▶ sysproc.c: sbi_shutdown()
                                ──▶ riscv.h: ecall (SBI_SHUTDOWN)  [S-mode → M-mode]
                                      ──▶ OpenSBI → QEMU 退出
```

**路径二：外部程序**（fork + exec，作为备用）
```
shell 中输入 "halt"
  ──▶ fork() → exec("/halt") → user/halt.c: main()
        ──▶ printf("shutting down...") → halt()
              ──▶ [后续同上]
```

#### 为什么 halt 要做成 shell 内建命令？

| 对比 | fork/exec 方式 | shell 内建方式 |
|------|---------------|---------------|
| 需要 `_halt` ELF 文件 | ✅ 必须存在于 fs.img | ❌ 不需要（即使文件损坏也能关机） |
| 需要 fork 子进程 | ✅ 需要 | ❌ 直接在 shell 进程中执行 |
| 设计参照 | 普通命令如 `echo`、`ls` | `cd` — 必须在父进程中执行 |

`cd` 不能 fork 是因为子进程改目录不影响父进程。`halt` 不需要 fork，而且内建确保即使文件系统出问题也能关机。

#### shell 内建检测代码

**`user/sh.c`**（在 `main()` 的 while 循环中，`cd` 检测之前）：
```c
if (cmd[0] == 'h' && cmd[1] == 'a' && cmd[2] == 'l' && cmd[3] == 't' &&
    (cmd[4] == '\n' || cmd[4] == ' ' || cmd[4] == '\0')) {
  // Shutdown: call halt() directly in the shell, no need to fork.
  printf("shutting down...\n");
  halt();
}
```

**关键点**：
- `halt` 放在 `cd` 检测**之前**，因为 `halt` 不需要 fork（与 `cd` 同理）
- 条件同时匹配 `halt`、`halt\n`、`halt ` 三种情况
- 调用 `halt()` 后机器直接关机，所以不需要 `exit(0)`

#### 各层代码

**SBI 关机**（`kernel/riscv.h`）：
```c
static inline void
sbi_shutdown(void) {
  asm volatile("li a7, 0x08\n\t"   // SBI legacy extension: SHUTDOWN
               "li a6, 0\n\t"      // function ID = 0
               "ecall");           // S-mode → M-mode
}
```

RISC-V SBI 规范：
- `a7 = 0x08`：legacy SHUTDOWN 扩展
- `a6 = 0`：无特定功能号
- `ecall`：从 S-mode 触发 trap 到 M-mode（OpenSBI）

**内核实现**（`kernel/sysproc.c`）：
```c
uint64 sys_halt(void) {
  sbi_shutdown();   // 调用 SBI 关机
  return 0;
}
```

**用户态 stub**（`user/usys.pl` 自动生成）：
```asm
.global halt
halt:
  li a7, SYS_halt   # 系统调用号 23
  ecall              # 陷入内核
  ret
```

**独立用户程序**（`user/halt.c`）：
```c
int main(void) {
  printf("shutting down...\n");
  halt();
  exit(0);
}
```

---

### 四、遇到的关键问题与修复

#### 问题：`ecall` 未能到达 OpenSBI，内核 panic

**现象**：
```
scause=0x9 sepc=0x800039ec stval=0x0
panic: kerneltrap
```

**原因**：`start.c` 中 `w_medeleg(0xffff)` 将所有异常（包括 bit 9：Environment call from S-mode）委托给 S-mode。导致内核执行 `ecall` 时，trap 被内核自己的 `kerneltrap()` 捕获，OpenSBI 根本收不到请求。

**修复**：排除 bit 9 的委托
```c
// 修改前
w_medeleg(0xffff);

// 修改后
w_medeleg(0xffff & ~(1L << 9));  // bit 9 不委托，由 OpenSBI (M-mode) 处理
```

RISC-V `medeleg` 寄存器各 bit 含义：

| Bit | 异常 | 委托到 S-mode? |
|-----|------|---------------|
| 8 | Environment call from U-mode | ✅ 委托（syscall 需要） |
| 9 | Environment call from S-mode | ❌ 不委托（SBI 需要 M-mode 处理） |

---

#### 问题2：SBI ecall 发出后 QEMU 不退出的修复 ✅

**现象**：
```
$ halt
shutting down...
[QEMU 挂住，不退出，终端回不到 shell]
```

**原因**：xv6 使用 `-bios none -kernel` 启动，没有 OpenSBI 固件。内核在 `start()` 中从 M-mode 切换到 S-mode（`mret`）后，**没有设置 `mtvec`**（M-mode 陷阱向量）。当 `sbi_shutdown()` 执行 `ecall` 从 S-mode 进入 M-mode 时，`mtvec` 为默认值 0，CPU 跳转到地址 0 处执行（该地址在 QEMU virt 机器中是空/无效的），导致 CPU 挂死。

此外，即使 M-mode 陷阱能被正确路由，`-bios none` 下 QEMU 不会提供完整的 SBI 实现，legacy SHUTDOWN（EID 0x08）不会被处理。

**修复**：不再依赖 SBI ecall 关机，改用 **QEMU test device**（SiFive test device，位于物理地址 `0x100000`）直接退出 QEMU。向该 MMIO 地址写入任意值即可触发 QEMU 退出。

涉及 3 个文件的修改：

| 文件 | 修改内容 |
|------|---------|
| `kernel/memlayout.h` | 新增 `#define TEST_DEVICE 0x100000L` |
| `kernel/vm.c` | `kvmmake()` 中新增 identity mapping：`kvmmap(kpgtbl, TEST_DEVICE, TEST_DEVICE, PGSIZE, PTE_R \| PTE_W)` |
| `kernel/sysproc.c` | `sys_halt()` 改为 `*((volatile uint32*)TEST_DEVICE) = 0x5555`，替代原来的 `sbi_shutdown()` |

```c
// 修改前（sysproc.c）
uint64 sys_halt(void) {
  sbi_shutdown();   // ecall → M-mode → 无人处理 → QEMU 挂死
  return 0;
}

// 修改后（sysproc.c）
uint64 sys_halt(void) {
  volatile uint32 *test_dev = (volatile uint32 *)TEST_DEVICE;
  *test_dev = 0x5555;  // 直接写 QEMU test device → QEMU 退出
  return 0;
}
```

**原理**：QEMU `virt` 机器内置了一个 SiFive test device（位于 `hw/misc/sifive_test.c`），其 MMIO 地址在 `0x100000`。向该地址写入时，QEMU 调用 `qemu_system_shutdown_request_with_code()` 优雅退出。只需在内核页表中将该物理地址做 identity mapping（因为 xv6 内核使用直接映射，VA == PA），S-mode 内核代码就可以直接写该地址。

| 地址范围 | 设备 | 说明 |
|---------|------|------|
| `0x00001000` | boot ROM | QEMU 提供的引导 ROM |
| `0x00100000` | **test device** | **写此地址退出 QEMU** |
| `0x02000000` | CLINT | 核间中断和定时器 |
| `0x0C000000` | PLIC | 平台级中断控制器 |
| `0x10000000` | UART0 | 串口 |
| `0x10001000` | virtio disk | 磁盘 |

---

### 五、涉及 xv6 机制

| 机制 | 说明 | 涉及文件 |
|------|------|---------|
| 系统调用分发 | `syscalls[]` 函数指针表，按编号分发 | `kernel/syscall.c` |
| 用户态 stub 生成 | Perl 脚本自动生成汇编 stub（li a7, N; ecall; ret） | `user/usys.pl` |
| RISC-V ecall 指令 | U-mode → S-mode（系统调用）、S-mode → M-mode（SBI） | `user/usys.S`, `kernel/riscv.h` |
| SBI（Supervisor Binary Interface） | 内核与 OpenSBI 固件之间的接口 | `kernel/riscv.h` |
| `medeleg` 寄存器 | 控制异常委托策略 | `kernel/start.c` |
| Shell 内建命令 | 类似 `cd`，在 shell 进程中直接执行，不 fork | `user/sh.c` |

---

### 六、运行验证

```
$ make qemu
...
init: starting sh
$ halt
shutting down...
[QEMU 退出，返回终端]
houyiming@houyiming-VMware-Virtual-Platform:~/桌面/OS_xv6_riscv$
```

---

### 小结

> 完成 `halt` 关机系统调用的完整实现（用户态→内核态→QEMU test device 全链路），支持 shell 内建命令和独立可执行文件两种方式。遇到两个关键问题：① `medeleg` 委托 ecall from S-mode 导致内核 panic，通过排除 bit 9 修复；② SBI ecall 后 QEMU 不退出（因为 `-bios none` 下无 M-mode 陷阱处理），改用 QEMU test device（`0x100000`）直接退出实现。系统调用号 23，涉及 **10 个文件的修改/新建**。
