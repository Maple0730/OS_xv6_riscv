## tfc



## maple




## hym

### 1.tfc:动态切换调度算法

在 `kernel/sysproc.c` 中新增系统调用，允许用户态程序运行时切换调度策略：

```c
uint64
sys_schedtype(void)
{
  int type;
  argint(0, &type);
  if (type != SCHED_RR && type != SCHED_FCFS)
    return -1;
  sched_type = type;
  return 0;
}
```

对应在 `kernel/syscall.h` 中添加 `#define SYS_schedtype 23`，在 `kernel/syscall.c` 的 `syscalls[]` 中添加 `sys_schedtype` 条目。

### 2.Maple：文件系统支持标准输入输出重定向
  详见docx/message/xv6 文件系统标准输入输出重定向技术方案.md