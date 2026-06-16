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

### 共享内存机制完善

#### 内核修改

- `kernel/shm.h` - 扩展 `struct shm`（新增 forkcount 字段）
- `kernel/shm.c` - 完整实现 shmget / shmat / shmdt（含引用计数机制）
- `kernel/proc.c` - kfork 复制共享内存映射；freeproc 递减引用计数
- `kernel/vm.c` - uvmunmap 保护共享内存页面不被错误释放
- `kernel/proc.h` - 添加 `shm_shmidx`、`rqnext`/`rqprev`、统计字段
- `kernel/sysproc.c` - sys_shmget / sys_shmat / sys_shmdt 实现
- `kernel/syscall.h` - SYS_shmget=29, SYS_shmat=30, SYS_shmdt=31

#### 用户态接口

- `user/user.h` - shmget / shmat / shmdt 用户 API（含 IPC_CREAT 宏）
- `user/usys.pl` - 桩代码

#### 测试程序

- `user/shmtest.c` - 基础测试 / 父子通信测试 / fork 继承测试

### MLFQ 五级队列与调度统计

#### 内核修改

- `kernel/param.h` - MLFQ_LEVELS=5，新增 Q3/Q4 时间片常量
- `kernel/proc.c` - 5级MLFQ调度器（进程表扫描）、mlfq_boost_priority、调度统计收集
- `kernel/proc.h` - wait_time/run_time/sched_count（统计字段）
- `kernel/sysproc.c` - sys_schedstat 实现（SYS_schedstat=38）

#### 测试程序

- `user/schedstat.c` - 调度统计查看程序
- `user/schedlatency.c` - 调度延迟测试程序

### 高精度计时器

#### 内核修改

- `kernel/sysproc.c` - sys_cgettimeofday() 实现（SYS_cgettimeofday=37）

#### 测试程序

- `user/cgettime.c` - 微秒级计时器测试程序

### 性能测试增强

#### 测试程序

- `user/throughput.c` - 重写，三算法对比（RR/FCFS/MLFQ）
- `user/schedlatency.c` - 调度延迟测试（新增）

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
shmtest    # 共享内存测试
semtest1   # 基本信号量测试
semtest2   # 互斥锁测试
semtest3   # 生产者-消费者测试
waitpidtest # waitpid 系统调用测试
mlfqtest   # MLFQ 调度观测测试
schedtest  # 调度算法切换测试
schedstat  # 调度统计查看
cgettime   # 高精度计时器测试
throughput # 吞吐量对比测试
schedlatency # 调度延迟测试
ps         # 查看进程状态
```

---

## 参考文档

- 设计文档：`docx/tfc/FCFS_MLFQ_Scheduler_Design.md`
- 已完成任务：`docx/tfc/Done.md`
- 信号量实现详情：见 `kernel/sem.c`
- waitpid 实现详情：见 `kernel/proc.c`
- 共享内存实现详情：见 `kernel/shm.c`
