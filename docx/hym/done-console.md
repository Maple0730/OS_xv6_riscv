## console — 控制台输出原子化 ✅

> 负责人：侯奕明
> 完成日期：2026-06-09

---

### 一、概述

解决 xv6 控制台多进程输出字符级交错（interleaving）的问题。通过**内核 console 加 sleep lock** + **用户态 printf 动态缓冲**，使每次 `printf` 的输出变成原子操作，不再被其他进程的输出打断。

**对应文件**：`kernel/console.c`（修改）、`user/printf.c`（重写）

---

### 二、问题分析

#### 现象

Test 3 中父子进程同时向控制台 printf 时，输出逐字符交错：

```
修复前：
pipetest: papiprente sentte 'shtell:o c'h    ← 两个进程的字符混在一起
ildp ipmetsg est0: pa: rhent seellon

修复后：
pipetest: parent sent 'hello'               ← 每行完整独立
pipetest: child msg 0: hello
```

#### 根因链路

```
用户态                     内核态
───────                    ──────
printf("parent sent 'hello'\n")
  └→ putc('p')              └→ write(1, "p", 1)
       └→ write(fd, &c, 1)       └→ consolewrite() → uartwrite("p", 1)
  └→ putc('a')              └→ write(1, "a", 1)        【调度器可能在此切换】
       └→ write(fd, &c, 1)       └→ consolewrite() → uartwrite("a", 1)
  ...                         ...
```

**两个问题同时存在**：

| 层次 | 问题 | 后果 |
|------|------|------|
| 用户态 | `printf` 每字符调一次 `write()` | 一次 printf = N 次系统调用 |
| 内核态 | `consolewrite()` 无锁保护 | N 次 write 之间调度器可切进程 |

两个进程的字符就这样在时间轴上交替，最终输出在屏幕上完全混杂。

---

### 三、修复方案

#### 修改 1：内核 `console.c` — 添加 sleep lock

**文件**：`kernel/console.c`

**改动点**：

```c
// 1. cons 结构体新增 sleeplock
struct {
  struct spinlock lock;
  struct sleeplock writelock;     // ← 新增：序列化控制台写入
  ...
} cons;

// 2. consoleinit() 初始化 writelock
initsleeplock(&cons.writelock, "cons_write");   // ← 新增

// 3. consolewrite() 拿锁/放锁
acquiresleep(&cons.writelock);    // ← 新增：写前拿锁
  ... uartwrite() 批量输出 ...
releasesleep(&cons.writelock);    // ← 新增：写完放锁
```

**为什么用 sleep lock 而不是 spinlock？**

`uartwrite()` 内部可能因为 UART 发送缓冲区满而调用 `sleep()`。xv6 规定**持 spinlock 时不能 sleep**（会触发 `panic: sched locks`），所以必须用允许 sleep 的 `sleeplock`。

---

#### 修改 2：用户态 `printf.c` — 动态缓冲区 + 一次性 write

**文件**：`user/printf.c`

**2.1 初版方案（有缺陷）**：固定 256 字节栈缓冲区，满了就中途 flush。

```c
if (*len >= PRINTF_BUF_SIZE - 1) {
    write(fd, buf, *len);   // ← 中途写，原子性在此断裂
    *len = 0;
}
```

**为什么有缺陷？** 如果 printf 输出超过 256 字节，一次 printf 变成多次 write()，另一个进程可能插在中间输出，原子性失效。

**2.2 最终方案（动态扩容）**：引入 `struct bufstate`，先用栈缓冲区；超过容量时自动 `malloc` 双倍扩容，最终一次性 `write()`：

```c
struct bufstate {
  char *buf;      // 当前缓冲区指针
  int len;        // 已写入字节数
  int cap;        // 缓冲区总容量
  int on_heap;    // buf 是否来自 malloc（需要 free）
};

static void
bufputc(struct bufstate *bs, char c)
{
  if (bs->len >= bs->cap) {
    // 容量不足 → malloc 双倍扩容
    int newcap = bs->cap * 2;
    char *newbuf = malloc(newcap);
    if (newbuf == 0) {
      // malloc 失败：flush 已有内容，重置缓冲区
      write(1, bs->buf, bs->len);
      bs->len = 0;
      return;         // ← 仅内存耗尽时才会分裂
    }
    // 拷贝旧内容到新缓冲区
    for (int j = 0; j < bs->len; j++)
      newbuf[j] = bs->buf[j];
    if (bs->on_heap)
      free(bs->buf);
    bs->buf = newbuf;
    bs->cap = newcap;
    bs->on_heap = 1;
  }
  bs->buf[bs->len++] = c;
}

void vprintf(int fd, const char *fmt, va_list ap) {
  char stackbuf[256];                // 默认用栈缓冲
  struct bufstate bs = {
    .buf = stackbuf,
    .len = 0, .cap = 256, .on_heap = 0
  };
  // ... 格式化过程 ...
  if (bs.len > 0)
    write(fd, bs.buf, bs.len);      // 一次性输出 ← 原子
  if (bs.on_heap)
    free(bs.buf);                    // 释放 malloc 的内存
}
```

**设计决策**：

| 场景 | 行为 | 原子性 |
|------|------|:------:|
| printf 输出 ≤ 256 字节 | 栈缓冲区直接写 | ✅ 原子 |
| printf 输出 > 256 字节 | 自动 malloc 扩容到足够大 | ✅ 原子 |
| malloc 失败（内存耗尽） | flush 已有内容，继续用原缓冲 | ⚠️ 会分裂 |

> xv6 中 printf 几乎不会超过 256 字节（典型输出 30-80 字节），前两种情况覆盖所有正常场景。第三种是极端情况，此时系统已接近崩溃，分裂输出可以接受。

---

### 四、两层保护协同工作

```
用户态 printf
  └→ 格式化到 bufstate（栈或 malloc）
  └→ 调用一次 write(fd, buf, len)       ← 整个 printf 一次 write
                            ↓
内核态 consolewrite()
  └→ acquiresleep(&cons.writelock)     ← 拿锁
  └→ 批量 uartwrite() 全部字符
  └→ releasesleep(&cons.writelock)     ← 放锁
```

- **用户态动态缓冲**：确保一次 `printf` = 一次 `write()`（即使超过 256 字节也能自动扩容）
- **内核态锁**：确保一次 `write()` 期间不会被其他进程的 `write()` 插入

两层保护使每次 `printf` 成为一个**原子操作**。

---

### 五、修改文件清单

| 文件 | 修改内容 |
|------|---------|
| `kernel/console.c` | `cons` 结构体新增 `sleeplock writelock` |
| | `consoleinit()` 调用 `initsleeplock()` |
| | `consolewrite()` 首尾加 `acquiresleep/releasesleep` |
| `user/printf.c` | 重写 `vprintf()`：`struct bufstate` + 动态扩容 + 一次性 `write()` |

---

### 六、涉及 xv6 机制

| 机制 | 说明 | 涉及文件 |
|------|------|---------|
| sleep lock | 允许持锁时 sleep 的锁，用于保护可能阻塞的临界区 | `kernel/sleeplock.c` |
| spinlock vs sleeplock 区别 | spinlock 禁止 sleep（用于中断/短临界区），sleeplock 允许 sleep（用于 I/O 等待） | `kernel/spinlock.c` |
| 动态内存 (user malloc) | 用户态 `malloc/free`，基于 `sbrk`，用于 printf 缓冲区扩容 | `user/umalloc.c` |
| UART 输出 | 中断驱动的串口输出，发送缓冲区满时 sleep | `kernel/uart.c` |
| console 设备 | 通过 `devsw[CONSOLE]` 注册 read/write 函数 | `kernel/console.c` |
| 系统调用 write 路径 | `sys_write → filewrite → consolewrite → uartwrite` | `kernel/sysfile.c` |

---

### 七、验证

```
$ pipetest
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
```

每行输出完整独立，不再出现字符交错。

> **注**：msg 3 比 `parent sent 'test'` 先出现是因为子进程先被调度执行，这不影响正确性——管道数据传递本身是正确的，printf 的执行顺序取决于调度器。

---

### 小结

> 通过内核 console sleep lock + 用户态 printf 动态缓冲两层改造，实现了控制台输出的原子化。初版固定 256 字节缓冲有中途 flush 导致原子性断裂的风险，最终版改用动态扩容机制，确保任意大小的 printf 都只产生一次 write()。解决了多进程并发输出时的字符级交错问题，使调试输出清晰可读。
