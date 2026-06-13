# 待完成任务 (Todo.md)

## 进程管理和处理器调度

---

## 已完成

### 信号量机制实现

#### 内核修改

- `kernel/sem.h` - 信号量数据结构定义
- `kernel/sem.c` - 内核信号量函数实现 (sem_init, sem_wait, sem_post, sem_get, sem_close)
- `kernel/main.c` - 添加 seminit() 调用
- `kernel/defs.h` - 添加信号量函数声明
- `kernel/syscall.h` - 添加系统调用编号 (SYS_sem_open ~ SYS_sem_close)
- `kernel/sysproc.c` - 实现信号量系统调用入口
- `kernel/syscall.c` - 添加分发条目
- `Makefile` - 添加 sem.o 编译目标

#### 用户态接口

- `user/sem.h` - 用户API头文件
- `user/sem.c` - 用户态包装函数
- `user/usys.pl` - 添加桩代码生成

#### 测试程序

- `user/semtest1.c` - 基本 P/V 操作测试
- `user/semtest2.c` - 互斥锁测试
- `user/semtest3.c` - 生产者-消费者测试

### waitpid 系统调用实现

#### 内核修改

- `kernel/proc.c` - 添加 `kwaitpid(int pid, uint64 addr)` 函数
- `kernel/sysproc.c` - 添加 `sys_waitpid()` 系统调用入口
- `kernel/defs.h` - 添加 `kwaitpid` 函数声明
- `kernel/syscall.h` - 添加系统调用编号 `SYS_waitpid` (33)
- `kernel/syscall.c` - 添加分发条目

#### 用户态接口

- `user/user.h` - 添加 `waitpid(int pid, int *status)` 函数原型
- `user/usys.pl` - 添加桩代码生成
- `Makefile` - 添加 `_waitpidtest` 编译目标

#### 测试程序

- `user/waitpidtest.c` - waitpid 功能测试

---

## 未完成

### 共享内存机制

| 项目 | 说明 |
|------|------|
| **当前状态** | 部分完成 / 临时回退 |
| **说明** | 已实现 `shmget` / `shmat` / `shmdt` 相关接口与内核框架，但为修复测试页错误，当前 `sys_shm*` 均返回 `-1`，测试程序已改为不依赖共享内存的版本 |
| **下一步** | 定位 `SHM_BASE` 页表映射 / `kfork` 复制共享页的异常，恢复用户态共享内存 |

### MLFQ 可观测性增强

#### 内核修改

- `kernel/proc.c` - 添加 `[MLFQ]` 日志：
  - `mlfq_enqueue()` - 进程入队时记录
  - `mlfq_boost_priority()` - 优先级提升时记录
  - `mlfq_scheduler()` - 调度选择时记录
  - `yield()` - 主动让出时降级记录

- `kernel/trap.c` - 添加 `[MLFQ]` 日志：
  - 时钟中断处理 - 时间片用完降级时记录

#### 测试程序

- `user/mlfqtest.c` - MLFQ 调度观测测试（已存在）

---

## 未完成

### 共享内存机制

| 项目 | 说明 |
|------|------|
| **当前状态** | 部分完成 / 临时回退 |
| **说明** | 已实现 `shmget` / `shmat` / `shmdt` 相关接口与内核框架，但为修复测试页错误，当前 `sys_shm*` 均返回 `-1`，测试程序已改为不依赖共享内存的版本 |
| **下一步** | 定位 `SHM_BASE` 页表映射 / `kfork` 复制共享页的异常，恢复用户态共享内存 |

### 性能评测增强

| 项目 | 说明 |
|------|------|
| **当前状态** | 未完成 |
| **说明** | `throughput` 仍分辨率不足，难以做严格横向比较 |

---

## 测试验证

### 编译与运行

```bash
cd /home/tfc/OS/OS_xv6_riscv
make clean && make
make qemu
```

### 在 xv6 shell 中运行测试

```bash
semtest1    # 基本信号量测试
semtest2    # 互斥锁测试
semtest3    # 生产者-消费者测试
waitpidtest # waitpid 系统调用测试
mlfqtest    # MLFQ 调度观测测试
ps          # 查看进程状态
```

---

## 参考文档

- 设计文档：`docx/tfc/FCFS_MLFQ_Scheduler_Design.md`
- 已完成任务：`docx/tfc/Done.md`
- 信号量实现详情：见 `kernel/sem.c`
- waitpid 实现详情：见 `kernel/proc.c`
