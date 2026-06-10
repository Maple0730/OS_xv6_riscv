## pipetest — 管道通信测试程序 ✅

> 负责人：侯奕明
> 完成日期：2026-06-09

---

### 一、概述

编写了 `user/pipetest.c` 管道通信测试程序，用于验证 xv6 管道机制的正确性。覆盖基本 ping-pong、大块数据传输（环形缓冲区回绕）和多消息收发三个测试场景。

**对应文件**：`user/pipetest.c`（新建）

---

### 二、测试场景设计

| 测试 | 名称 | 验证点 | 数据量 |
|:----:|------|--------|--------|
| Test 1 | 基本 ping-pong | 父子进程双向通信 | 4 字节 × 2 |
| Test 2 | 大量数据传输 | 环形缓冲区回绕（>512 字节） | 768 字节 |
| Test 3 | 多消息收发 | 连续多次小消息读写 | 5 条消息 |

#### Test 1: 基本 ping-pong

```
父进程 ──write("ping")──→ p1[管道] ──read()──→ 子进程
父进程 ←─read()───────── p2[管道] ←─write("pong")── 子进程
```

- 父进程写 "ping" 到 p1，关闭写端；子进程读到后打印
- 子进程写 "pong" 到 p2；父进程读到后打印
- 验证：双向管道通信、close 后 read 返回 0、wait 同步

#### Test 2: 大量数据传输（768 字节）

- 父进程填充已知模式数据（`buf[i] = i & 0xff`），一次性 write 768 字节
- 子进程循环 read 直到返回 0，累计 total
- 验证：管道环形缓冲区满时正确回绕（xv6 PIPESIZE = 512）
- 验证：total == 768，数据无丢失

#### Test 3: 多消息收发

- 父进程循环写 5 条消息："hello", "xv6", "pipe", "test", "world"
- 子进程循环读 5 次，每次打印读取内容
- 验证：连续多次 read/write 消息边界正确

---

### 三、修复：`%.*s` 格式符兼容性问题

#### 问题

xv6 的 `printf`（`user/printf.c`）**不支持** `%.*s`（带精度的字符串格式）。

当遇到 `%.*s` 时，`.` 不是合法格式符，走 `else` 分支直接输出字面量 `%.*s`：

```
# 修复前输出：
pipetest: child received: %.*s    ← 字面量，不是实际内容
```

#### 修复方案

用 `%s` 代替 `%.*s`，read 后手动添加 `\0` 终止符：

```c
// 修复前
printf("pipetest: child received: %.*s\n", n, buf);

// 修复后
buf[n] = '\0';
printf("pipetest: child received: %s\n", buf);
```

#### 涉及位置

| 位置 | 函数 | 修复内容 |
|------|------|---------|
| 原第 37 行 | Test 1 子进程 | `%.*s` → `buf[n]='\0'` + `%s` |
| 原第 57 行 | Test 1 父进程 | 同上 |
| 原第 114 行 | Test 3 子进程 | 同上 |

---

### 四、运行结果

```
$ pipetest
pipetest: start
pipetest: Test 1 -- ping-pong
pipetest: child received: ping
pipetest: parent received: pong
pipetest: Test 1 -- PASS
pipetest: Test 2 -- large data (768 bytes)
pipetest: parent wrote 768 bytes
pipetest: child read total 768 bytes
pipetest: Test 2 -- PASS
pipetest: Test 3 -- multiple messages
pipetest: parent sent 'hello'
pipetest: child msg 0: hello
pipetest: parent sent 'xv6'
pipetest: child msg 1: xv6
pipetest: parent sent 'pipe'
pipetest: child msg 2: pipe
pipetest: child msg 3: test
pipetest: parent sent 'test'
pipetest: parent sent 'world'
pipetest: child msg 4: world
pipetest: Test 3 -- PASS
pipetest: ALL TESTS PASSED
```

---

### 五、涉及 xv6 机制

| 机制 | 系统调用 | 涉及文件 |
|------|---------|---------|
| 管道创建 | `pipe(p1)`, `pipe(p2)` | `kernel/pipe.c` |
| 进程创建 | `fork()` | `kernel/proc.c` |
| 管道读写 | `read()`, `write()` | `kernel/pipe.c`, `kernel/file.c` |
| 文件描述符管理 | `close()` | `kernel/file.c` |
| 等待子进程 | `wait(0)` | `kernel/proc.c` |
| 管道缓冲管理 | 环形缓冲区 (PIPESIZE=512) | `kernel/pipe.c` |

---

### 六、修复：Test 3 管道字节流消息合并问题

#### 问题发现

修复 console 输出交错后，发现 Test 3 的输出**不稳定**：

```
第 1 次运行：msg 2: pipetest, msg 3: world      ← 消息粘连
第 2 次运行：msg 0-4 全部正确                     ← 运气好
第 3 次运行：msg 2: pipetest, msg 3: world      ← 又粘连了
```

#### 为什么会出现？

**管道是字节流，不保留消息边界。** 父进程连续 `write()` 5 条消息之间没有分隔符：

```
父进程: write("hello") write("xv6") write("pipe") write("test") write("world")
管道内容: helloxv6pipetestworld   ← 无边界标记
子进程: read() 可能读到 "helloxv6" 或 "pipe" 或 "testworld" … 完全取决于调度时机
```

之前这个 bug 被 console 输出交错**掩盖**了（混乱的输出中无法辨认），console 修复后才暴露出来。

#### 修复方案

添加 `\n` 作为应用层消息分隔符，子进程逐字节读直到 `\n` 再进行输出：

```c
// 父进程：每条消息尾部追加 '\n'
write(p1[1], msgs[i], strlen(msgs[i]));
write(p1[1], "\n", 1);   // ← 消息分隔符

// 子进程：逐字节读，遇 '\n' 输出一行（应用层消息分帧）
int msgcount = 0, linepos = 0;
char c;
while (msgcount < 5) {
  n = read(p1[0], &c, 1);
  if (n <= 0) break;
  if (c == '\n') {
    buf[linepos] = '\0';
    printf("pipetest: child msg %d: %s\n", msgcount, buf);
    msgcount++;
    linepos = 0;
  } else if (linepos < (int)sizeof(buf) - 1) {
    buf[linepos++] = c;
  }
}
```

#### 效果

连续 3 次运行，每次 msg 0-4 都是正确分离的 `hello/xv6/pipe/test/world`：

```
=== 第1次 ===          === 第2次 ===          === 第3次 ===
msg 0: hello    ✅     msg 0: hello    ✅     msg 0: hello    ✅
msg 1: xv6      ✅     msg 1: xv6      ✅     msg 1: xv6      ✅
msg 2: pipe     ✅     msg 2: pipe     ✅     msg 2: pipe     ✅
msg 3: test     ✅     msg 3: test     ✅     msg 3: test     ✅
msg 4: world    ✅     msg 4: world    ✅     msg 4: world    ✅
```

---

### 七、修改文件汇总

| 文件 | 阶段 | 改动 |
|------|:----:|------|
| `user/pipetest.c` | 新建 | 完整测试程序（Test 1/2/3） |
| `user/pipetest.c` | 修复1 | `%.*s` → `buf[n]='\0'` + `%s`（3处） |
| `user/pipetest.c` | 修复2 | Test 3 用 `\n` 分隔符 + 逐字节读分帧 |

---

### 小结

> 完成管道通信测试程序的编写与调试，覆盖三类核心场景（ping-pong、大数据、多消息）。修复两处问题：(1) xv6 printf 不兼容 `%.*s`；(2) Test 3 未考虑管道字节流特性导致消息合并。所有测试通过，输出稳定。
