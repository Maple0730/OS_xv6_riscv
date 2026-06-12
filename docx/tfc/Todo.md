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

---

## 未完成

### 共享内存机制

| 项目 | 说明 |
|------|------|
| **当前状态** | 部分完成 / 临时回退 |
| **说明** | 已实现 `shmget` / `shmat` / `shmdt` 相关接口与内核框架，但为修复测试页错误，当前 `sys_shm*` 均返回 `-1`，测试程序已改为不依赖共享内存的版本 |
| **下一步** | 定位 `SHM_BASE` 页表映射 / `kfork` 复制共享页的异常，恢复用户态共享内存 |

### waitpid / 子进程链表

| 项目 | 说明 |
|------|------|
| **当前状态** | 未完成 |
| **说明** | 当前仅支持父指针与 `wait()`，未实现 `waitpid` 以及子进程链表 |

### MLFQ 可观测性增强

| 项目 | 说明 |
|------|------|
| **当前状态** | 未完成 |
| **说明** | 现有测试只能从现象推断降级/提升，缺少直接日志 |

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
ps          # 查看进程状态
```

---

## 参考文档

- 设计文档：`docx/tfc/FCFS_MLFQ_Scheduler_Design.md`
- 已完成任务：`docx/tfc/Done.md`
- 信号量实现详情：见 `kernel/sem.c`
