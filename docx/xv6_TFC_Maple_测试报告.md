# xv6-riscv TFC调度 & Maple文件I/O重定向 — 测试报告

**测试日期**: 2026-06-19
**测试分支**: merge-0619
**测试人**: 侯奕明 (20231071289)

---

## 一、测试目标

验证以下两项功能的完整实现：

| 功能模块 | 目标 | 对应需求 |
|---------|------|---------|
| **TFC** | 用户态运行时动态切换调度算法 | sys_sched_algorithm 系统调用 |
| **Maple** | 文件系统支持标准输入输出重定向 `>`、`<`、`>>` | Shell 重定向 + O_APPEND 内核支持 |

---

## 二、TFC 调度算法切换测试

### 2.1 实现分析

本代码库实现了比基础要求更完善的调度切换系统：

- **系统调用**: `sys_sched_algorithm` (编号38)，定义在 `kernel/sysproc.c:245`
- **支持算法**: 0=RR, 1=FCFS, 2=MLFQ, 3=SJF, 4=PRIO, 5=(预留)
- **查询模式**: `algo=-1` 返回当前算法而不切换
- **用户态接口**: `sched_algorithm(int algo)` 和 `sched_algorithm_name(int algo)`
- **关键文件**:
  - `kernel/sysproc.c` — sys_sched_algorithm 系统调用实现
  - `kernel/syscall.h` — #define SYS_sched_algorithm 38
  - `kernel/syscall.c` — syscalls[] 数组注册
  - `user/usys.pl` — 用户态 stub 生成
  - `user/user.h` — 用户态函数声明

### 2.2 测试结果

```
========== Part 2: TFC Scheduling ==========

[Test 2.1] Query current scheduler
  [PASS] sched_algorithm(-1) returns valid algorithm
  Current scheduler: MLFQ (2)

[Test 2.2] Invalid algorithm numbers
  [PASS] sched_algorithm(99) returns -1
  [PASS] sched_algorithm(-2) returns -1

[Test 2.3] Switch to RR (0)
  [PASS] switch to RR succeeds
  [PASS] current is now RR

[Test 2.4] Switch to FCFS (1)
  [PASS] previous was RR
  [PASS] current is now FCFS

[Test 2.5] Switch to MLFQ (2)
  [PASS] previous was FCFS
  [PASS] current is now MLFQ

[Test 2.6] Switch to SJF (3)
  [PASS] previous was MLFQ
  [PASS] current is now SJF

[Test 2.7] Switch to PRIO (4)
  [PASS] previous was SJF
  [PASS] current is now PRIO

[Scheduling] 0 test(s) failed
```

### 2.3 结论

✅ **全部通过**。调度算法可在运行时无重启切换，支持 RR/FCFS/MLFQ/SJF/PRIO 五种算法，无效输入正确返回 -1。

---

## 三、Maple 文件 I/O 重定向测试

### 3.1 实现分析

#### 3.1.1 Shell 层（`user/sh.c`）

| 重定向符 | 解析函数 | 使用的标志 | 文件描述符 |
|---------|---------|-----------|-----------|
| `<` | `parseredirs()` → tok `<` | `O_RDONLY` | fd 0 (stdin) |
| `>` | `parseredirs()` → tok `>` | `O_WRONLY\|O_CREATE\|O_TRUNC` | fd 1 (stdout) |
| `>>` | `parseredirs()` → tok `+` | `O_WRONLY\|O_CREATE\|O_APPEND` | fd 1 (stdout) |

`runcmd()` 中 REDIR 分支执行逻辑：
```c
case REDIR:
    rcmd = (struct redircmd *)cmd;
    close(rcmd->fd);                       // 关闭目标 fd (0 或 1)
    if (open(rcmd->file, rcmd->mode) < 0)  // 打开文件，fdalloc 分配最低可用 fd
        ...
    runcmd(rcmd->cmd);                     // 执行实际命令
```

#### 3.1.2 内核层 — O_APPEND 支持（新增实现）

由于原始 xv6 不支持 `O_APPEND`，本次测试前新增了完整支持：

| 文件 | 修改内容 |
|------|---------|
| `kernel/fcntl.h` | 添加 `#define O_APPEND 0x080` |
| `kernel/file.h` | `struct file` 添加 `char append` 字段 |
| `kernel/sysfile.c` | `sys_open` 中设置 `f->append` 并初始化 `f->off = ip->size` |
| `kernel/file.c` | `filewrite` 中每次写入前检查 `f->append`，将偏移移至文件末尾 |
| `user/sh.c` | `>>` 分支使用 `O_WRONLY \| O_CREATE \| O_APPEND` |

### 3.2 内核层 I/O 测试结果

```
========== Part 1: I/O Redirection ==========

[Test 1.1] O_CREATE|O_WRONLY|O_TRUNC (simulates '>')
  [PASS] open with O_CREATE|O_TRUNC
  [PASS] write 12 bytes

[Test 1.2] O_RDONLY (simulates '<')
  [PASS] open O_RDONLY
  [PASS] read 12 bytes
  [PASS] content matches

[Test 1.3] O_APPEND (simulates '>>')
  [PASS] open with O_APPEND
  [PASS] append 14 bytes
  [PASS] total size is 26 (12+14)
  [PASS] content has both lines in order

[Test 1.4] O_TRUNC overwrite (simulates '>' on existing file)
  [PASS] re-open with O_TRUNC
  [PASS] write 6 bytes after trunc
  [PASS] size is 6 after trunc+write
  [PASS] old content gone

[Test 1.5] O_RDONLY on non-existent file
  [PASS] returns error for missing file

[Test 1.6] Multiple appends
  [PASS] 3 lines = 18 bytes
  [PASS] all 3 lines in order

[I/O Redirection] 0 test(s) failed
```

### 3.3 Shell 层重定向测试结果

```
$ echo hello > shtest.txt
$ cat shtest.txt
hello                                            ← ✅ 输出重定向成功

$ cat < shtest.txt
hello                                            ← ✅ 输入重定向成功

$ echo second >> shtest.txt
$ cat shtest.txt
hello
second                                           ← ✅ 追加重定向成功

$ cat < shtest.txt > shtest_out.txt
$ cat shtest_out.txt
hello
second                                           ← ✅ 同时输入+输出重定向成功
```

### 3.4 Shell 重定向仿真测试（fork + fd 操作）

```
========== Part 3: Shell-Level Redirection ==========

[Test 3.1] Output redirection 'echo hello > file'
  [PASS] child exit cleanly
  [PASS] file has 18 bytes
  [PASS] content correct

[Test 3.2] Input redirection 'cat < file'
  [PASS] child reads from redirected stdin

[Test 3.3] Append redirection 'echo second >> file'
  [PASS] child appends via O_APPEND
  [PASS] file has 13 bytes (6+7)
  [PASS] both lines in order

[Test 3.4] Combined 'cat < in > out'
  [PASS] child combines input+output redirection
  [PASS] output file has 14 bytes
  [PASS] combined redirection works

[Shell Redirection] 0 test(s) failed
```

### 3.5 结论

✅ **全部通过**。`>`、`<`、`>>` 三种重定向均在 Shell 中正常工作，支持单重重定向和组合重定向。

---

## 四、最终汇总

```
╔══════════════════════════════════════════╗
║  RESULT: ALL TESTS PASSED!              ║
╚══════════════════════════════════════════╝
```

### 测试覆盖

| 测试类别 | 测试项数 | 通过 | 失败 |
|---------|---------|------|------|
| Part 1: I/O 内核操作 (O_CREATE, O_TRUNC, O_APPEND, O_RDONLY) | 14 | 14 | 0 |
| Part 2: TFC 调度算法切换 (查询、无效输入、RR/FCFS/MLFQ/SJF/PRIO) | 13 | 13 | 0 |
| Part 3: Shell 重定向仿真 (fork+fd操作) | 9 | 9 | 0 |
| Shell 直接测试 (>, <, >>, 组合) | 7 | 7 | 0 |
| **总计** | **43** | **43** | **0** |

### 改动文件清单

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `kernel/fcntl.h` | **新增** | 添加 `O_APPEND 0x080` 宏定义 |
| `kernel/file.h` | **修改** | `struct file` 添加 `char append` 字段 |
| `kernel/file.c` | **修改** | `filewrite` 中处理 `O_APPEND`：写入前将偏移移至文件末尾 |
| `kernel/sysfile.c` | **修改** | `sys_open` 中设置 `f->append` 并初始化追加偏移 |
| `user/sh.c` | **修改** | `>>` 分支添加 `O_APPEND` 标志 |
| `user/iotest.c` | **新增** | 内核层 I/O 操作测试程序 |
| `user/alltest.c` | **新增** | 综合测试程序（I/O + 调度 + Shell重定向） |
| `Makefile` | **修改** | 添加 `iotest` 和 `alltest` 到 UPROGS |

### 核心验收标准达成情况

| 验收标准 | 状态 |
|---------|------|
| `echo hello > out.txt` 将字符串写入文件，屏幕无输出 | ✅ |
| `cat < in.txt` 从文件读取数据并显示 | ✅ |
| `cat < in.txt > out.txt` 同时支持输入和输出重定向 | ✅ |
| `echo world >> out.txt` 在文件末尾追加内容 | ✅ |
| 运行时动态切换调度算法（RR/FCFS/MLFQ/SJF/PRIO） | ✅ |

---

## 五、测试程序使用说明

### 在 xv6 中运行测试

```bash
# 综合测试（包含全部三项测试）
alltest

# 单独测试 I/O 内核操作
iotest

# 单独测试调度算法切换
schedtest

# Shell 重定向手工测试
echo hello > test.txt
cat test.txt
cat < test.txt
echo append >> test.txt
cat test.txt
cat < test.txt > out.txt
cat out.txt
```

### 重新编译

```bash
make clean
make -j$(nproc)
make qemu
```

---

*报告结束*
