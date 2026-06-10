## ps — 新增 ps 系统调用 ✅

> 负责人：侯奕明
> 完成日期：2026-06-09

---

### 一、概述

新增 `ps` 系统调用（编号 22），允许用户程序从用户态打印所有进程的状态信息。直接复用内核已有的 `procdump()` 函数，该函数原本只能通过 Ctrl-P 在内核态触发，现在通过系统调用暴露给用户态。

**涉及文件**：6 个文件（1 新建 + 5 修改）

---

### 二、修改文件清单

| 文件 | 修改内容 | 具体操作 |
|------|---------|---------|
| `kernel/syscall.h` | 新增系统调用号 | `#define SYS_ps 22` |
| `kernel/syscall.c` | 注册系统调用 | 新增 `extern uint64 sys_ps(void);` 声明 + `syscalls[]` 表中注册 `[SYS_ps] sys_ps` |
| `kernel/sysproc.c` | 新增 `sys_ps()` 函数 | 99-103 行，直接调用 `procdump()` |
| `user/usys.pl` | 生成用户态 stub | 新增 `entry("ps");` |
| `user/user.h` | 新增函数声明 | `int ps(void);` |
| `user/ps.c` | **新建**用户程序 | 调用 `ps()` 后 `exit(0)` |

---

### 三、实现原理

```
用户程序 ps.c: ps()
  → user/usys.pl 生成的 stub: li a7, SYS_ps; ecall; ret
  → trap.c:usertrap() 捕获 ecall
  → syscall.c:syscall() 查 syscalls[] 表，分发到 sys_ps()
  → sysproc.c:sys_ps() → procdump()
  → 遍历 proc[NPROC]，打印每个非 UNUSED 进程的 pid、state、name
```

#### 各层代码

**用户态**（`user/ps.c`）：
```c
int main(int argc, char *argv[]) {
  ps();
  exit(0);
}
```

**用户态 stub**（`user/usys.pl` 自动生成）：
```asm
# usys.pl 中 entry("ps") 生成：
.global ps
ps:
  li a7, SYS_ps   # 系统调用号 22
  ecall            # 陷入内核
  ret
```

**内核分发表**（`kernel/syscall.c`）：
```c
extern uint64 sys_ps(void);         // 声明

static uint64 (*syscalls[])(void) = {
  // ...
  [SYS_ps]    sys_ps,               // 注册
};
```

**内核实现**（`kernel/sysproc.c`）：
```c
uint64 sys_ps(void) {
  procdump();    // 直接复用内核已有的 procdump()
  return 0;
}
```

`procdump()`（`kernel/proc.c`）遍历 `proc[NPROC]`，打印每个 `state != UNUSED` 进程的 pid、state、name。

---

### 四、为什么这样设计

1. **零新增内核逻辑**：`sys_ps()` 只是对 `procdump()` 的一层包装，不修改任何进程管理逻辑
2. **最小侵入**：只改了 6 行代码（1 个 define + 1 个 extern + 1 个数组项 + 4 行新函数 + 1 行 perl entry + 1 行 .h 声明 + 10 行用户程序）
3. **系统调用号 22**：紧接原 21 个系统调用之后，保持编号连续
4. **用户态 vs 内核态触发**：
   - Ctrl-P → `consoleintr()` → `procdump()`（内核态，仅在调试时）
   - `ps` 命令 → `sys_ps()` → `procdump()`（用户态，任何时候）

---

### 五、涉及 xv6 机制

| 机制 | 说明 | 涉及文件 |
|------|------|---------|
| 系统调用分发 | `syscalls[]` 函数指针表，按编号分发 | `kernel/syscall.c` |
| 用户态 stub 生成 | Perl 脚本自动生成汇编 stub（li a7, N; ecall; ret） | `user/usys.pl` |
| ecall 陷入 | RISC-V ecall 指令触发陷阱，trampoline 切换至内核 | `kernel/trampoline.S`, `kernel/trap.c` |
| 进程状态遍历 | `proc[NPROC]` 数组，`procdump()` 打印各进程信息 | `kernel/proc.c` |

---

### 小结

> 完成 `ps` 系统调用的完整实现（用户态→内核态全链路），零新增内核逻辑，直接复用 `procdump()`。系统调用号 22，涉及 6 个文件的修改/新建。
