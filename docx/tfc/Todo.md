# 待完成任务 (Todo.md)

## 进程管理和处理器调度

---

## 已完成

### FCFS 与 MLFQ 调度算法实现

#### 内核修改

- `kernel/proc.h` - 添加调度相关字段
- `kernel/param.h` - 添加调度算法参数
- `kernel/proc.c` - 实现 RR/FCFS/MLFQ 调度器
- `kernel/trap.c` - 时间片用完处理
- `kernel/defs.h` - 函数声明

#### 测试程序

- `user/fcfstest.c` - FCFS 调度测试
- `user/mlfqtest.c` - MLFQ 调度测试
- `user/csw.c` - 上下文切换测试

---

## 待实施

### 1. 调度算法验证测试

**状态**: 待测试

**验证步骤**:
1. 使用 RR 调度器运行测试
2. 使用 FCFS 调度器运行测试
3. 使用 MLFQ 调度器运行测试
4. 对比分析结果

### 2. 性能测试

**状态**: 待测试

**测试内容**:
- 吞吐量测试
- 响应时间测试
- 上下文切换开销测试

### 3. 边界条件测试

**状态**: 待测试

**测试场景**:
- 进程数超过队列容量
- 优先级提升机制
- 多核调度

---

## 参考文档

- 设计文档：`docx/tfc/FCFS_MLFQ_Scheduler_Design.md`
- 已完成任务：`docx/tfc/Done.md`
- 测试指南：见 `docx/tfc/Done.md`
