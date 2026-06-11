# xv6-riscv lseek 系统调用实现

> 负责人：侯奕明  
> 对应 A 题模块：**3. 文件系统与设备驱动** — 文件操作增强  
> 更新日期：2026-06-11

---

## 一、概述

`lseek` 系统调用允许用户程序在打开的文件中**重新定位读写偏移量**（`f->off`），而不必从头到尾顺序读写。xv6 原始实现不支持 `lseek`，本次新增支持 POSIX 标准中定义的三种定位模式：`SEEK_SET`、`SEEK_CUR`、`SEEK_END`。

### 为什么需要 lseek？

| 场景 | 无 lseek | 有 lseek |
|------|---------|----------|
| 读取文件末尾 N 字节 | `read` 全文件后取最后 N 字节 | `lseek(-N, SEEK_END)` 直接定位 |
| 随机访问数据库记录 | 不支持 | `lseek(rec_offset, SEEK_SET)` 精确定位 |
| 获取文件大小 | 遍历整个文件统计字节数 | `lseek(0, SEEK_END)` 即得 |
| 跳过文件头 | 必须 read 丢弃不需要的数据 | `lseek(header_size, SEEK_SET)` 直接跳过 |

---

## 二、涉及文件与注册流程

### 2.1 文件清单

| 文件 | 作用 | 关键内容 |
|------|------|----------|
| `kernel/fcntl.h` | 定义 whence 常量 | `SEEK_SET=0`, `SEEK_CUR=1`, `SEEK_END=2` |
| `kernel/syscall.h` | 分配系统调用号 | `#define SYS_lseek 24` |
| `kernel/syscall.c` | 系统调用分发表 | `[SYS_lseek] sys_lseek` (第 136 行) |
| `kernel/sysfile.c` | 核心实现 | `sys_lseek()` (第 478–519 行) |
| `user/user.h` | 用户态函数声明 | `int lseek(int, int, int);` (第 29 行) |
| `user/usys.pl` | 用户态入口生成 | `entry("lseek");` (第 47 行) |
| `user/lseektest.c` | 测试程序 | 覆盖 SEEK_SET / SEEK_CUR / SEEK_END |
| `Makefile` | 编译链接 | `$U/_lseektest\` (第 182 行) |

### 2.2 调用路径

```
用户程序: lseek(fd, offset, whence)
    ↓
user/usys.S (自动生成):  li a7, SYS_lseek; ecall; ret
    ↓
kernel/trap.c:  scause=8 → syscall()
    ↓
kernel/syscall.c:  syscalls[24] → sys_lseek()
    ↓
kernel/sysfile.c:  sys_lseek()
    1. argfd(0)  → 取出 struct file *f
    2. argint(1) → offset
    3. argint(2) → whence
    4. ilock(f->ip) 获取 inode 锁
    5. switch (whence) 计算新偏移量
    6. iunlock(f->ip) 释放锁
    7. 返回新的 f->off
```

---

## 三、核心实现

### 3.1 whence 常量定义（`kernel/fcntl.h`）

```c
#define SEEK_SET 0  // 从文件开头定位
#define SEEK_CUR 1  // 从当前位置定位
#define SEEK_END 2  // 从文件末尾定位
```

### 3.2 sys_lseek() 实现（`kernel/sysfile.c:478-519`）

```c
uint64
sys_lseek(void)
{
  struct file *f;
  int offset, whence;

  if (argfd(0, 0, &f) < 0)
    return -1;
  argint(1, &offset);
  argint(2, &whence);

  if (f->type != FD_INODE)    // 只对普通文件支持 lseek
    return -1;

  ilock(f->ip);                // 加锁访问 inode

  switch (whence) {
  case SEEK_SET:
    if (offset < 0)            // SEEK_SET 不允许负偏移
      goto bad;
    f->off = offset;
    break;
  case SEEK_CUR:
    if (offset < 0 && f->off < (uint)(-offset))  // 防止下溢
      goto bad;
    f->off += offset;
    break;
  case SEEK_END:
    if (offset < 0 && f->ip->size < (uint)(-offset))  // 防止下溢
      goto bad;
    f->off = f->ip->size + offset;
    break;
  default:
    goto bad;
  }

  iunlock(f->ip);
  return f->off;    // 成功时返回新的偏移量

bad:
  iunlock(f->ip);
  return -1;        // 失败时返回 -1
}
```

### 3.3 三种定位模式详解

| 模式 | 常量 | 基准点 | 示例 | 结果 |
|------|------|--------|------|------|
| `SEEK_SET` | 0 | 文件开头 | `lseek(fd, 10, SEEK_SET)` | `f->off = 10` |
| `SEEK_CUR` | 1 | 当前位置 | `lseek(fd, -5, SEEK_CUR)` | `f->off -= 5` |
| `SEEK_END` | 2 | 文件末尾 | `lseek(fd, -6, SEEK_END)` | `f->off = size - 6` |

### 3.4 边界检查

```
SEEK_SET: offset < 0                         → 返回 -1
SEEK_CUR: offset < 0 && f->off < |offset|     → 返回 -1（防止负偏移）
SEEK_END: offset < 0 && f->ip->size < |offset| → 返回 -1（防止负偏移）
```

这些检查保证了 `f->off` 始终是非负整数，避免无符号整数下溢导致偏移量变成极大值。

### 3.5 与 read/write 的联动

`lseek` 修改的是 `struct file` 中的 `off` 字段（**文件偏移量**），而 `fileread()` 和 `filewrite()` 在执行 I/O 后会**自动推进** `f->off`：

```c
// file.c:137-138 — fileread 会自动推进偏移量
if ((r = readi(f->ip, 1, addr, f->off, n)) > 0)
    f->off += r;

// file.c:177-178 — filewrite 同样自动推进
if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
    f->off += r;
```

因此 `lseek` 设置的偏移量会在下一次 `read`/`write` 时被自动使用并更新。

---

## 四、测试程序（`user/lseektest.c`）

### 4.1 测试用例设计

| 步骤 | 操作 | 验证内容 |
|:----:|------|----------|
| 1 | 创建 16 字节文件 `"0123456789ABCDEF"` | 准备测试数据 |
| 2 | `lseek(10, SEEK_SET)` → 读 6 字节 | 从文件开头偏移 10 字节，读到 `"ABCDEF"` |
| 3 | `lseek(-6, SEEK_CUR)` → 读 6 字节 | 从当前位置（16）回退 6 → 回到 10，再读 `"ABCDEF"` |
| 4 | `lseek(-6, SEEK_END)` → 读 6 字节 | 从文件末尾回退 6 字节，读 `"ABCDEF"` |
| 5 | `lseek(0, SEEK_END)` | 返回值等于文件大小 16 |
| 6 | `unlink` 清理 | 删除测试文件 |

### 4.2 时序图

```
文件内容: "0123456789ABCDEF" (16 bytes)
偏移:      0         1
           0123456789ABCDEF
                      ^     ^
                      |     |
                    offset 10  (SEEK_SET)
                    "ABCDEF"

读"ABCDEF"后 → f->off = 16 (文件末尾)
    lseek(-6, SEEK_CUR) → f->off = 10
    再次读 → "ABCDEF"

    lseek(-6, SEEK_END) → f->off = size-6 = 10
    再次读 → "ABCDEF"

    lseek(0, SEEK_END) → f->off = 16 → 返回 16 = 文件大小
```

---

## 五、与 A 题模块的对应关系

本功能覆盖了 A 题**第 3 项"文件系统与设备驱动"**中的文件操作增强部分：

| 技术指标 | 状态 | 说明 |
|----------|:----:|------|
| 支持 SEEK_SET（从文件头定位） | ✅ | `whence=0`，检查 offset≥0 |
| 支持 SEEK_CUR（从当前位置定位） | ✅ | `whence=1`，允许正/负偏移 |
| 支持 SEEK_END（从文件末尾定位） | ✅ | `whence=2`，常用于获取文件大小 |
| 边界保护（防止负偏移） | ✅ | 三种模式均有下溢检查 |
| 锁保护（inode sleeplock） | ✅ | `ilock`/`iunlock` 包围关键区 |
| 错误返回（非普通文件） | ✅ | `f->type != FD_INODE` 时返回 -1 |
| 测试程序覆盖所有模式 | ✅ | `lseektest.c` 覆盖全部三种 whence |

---

## 六、设计要点总结

1. **POSIX 兼容**：`lseek(fd, offset, whence)` 接口与 POSIX 标准一致
2. **只作用于普通文件**：管道和设备的偏移量无意义，`f->type != FD_INODE` 直接返回 -1
3. **返回新偏移量**：成功时返回 `f->off`，与 Linux/POSIX 行为一致
4. **与 I/O 自动联动**：`read`/`write` 自动推进 `f->off`，无需额外处理
5. **取文件大小**：`lseek(fd, 0, SEEK_END)` 返回文件当前字节数，是最常用的技巧

> **核心思想**：`lseek` 只是修改 `struct file` 的 `off` 字段，真正的 I/O 由 `fileread`/`filewrite` 完成——它们会从 `f->off` 开始读写并自动推进偏移量。
