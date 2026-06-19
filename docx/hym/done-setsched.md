## done-setsched

### Shell 调度切换命令: `setsched`

新增 `user/setsched.c`，允许在 xv6 Shell 中随时查询/切换调度算法。

### 用法
```
$ setsched              # 查看当前调度算法
current scheduler: MLFQ (2)

$ setsched list         # 列出所有可用调度算法
  0 = RR     (Round Robin)
  1 = FCFS   (First Come First Served)
  2 = MLFQ   (Multi-Level Feedback Queue)
  3 = SJF    (Shortest Job First)
  4 = PRIO   (Static Priority)

$ setsched 1            # 切换到 FCFS
switched: MLFQ (2) -> FCFS (1)

$ setsched 0            # 切换到 RR
switched: FCFS (1) -> RR (0)
```

### 实现文件
| 文件 | 说明 |
|------|------|
| `user/setsched.c` | Shell 命令实现，调用 sched_algorithm() 系统调用 |
| `Makefile` | 添加 `_setsched` 到 UPROGS |

### 依赖的系统调用
- `sched_algorithm(int algo)` — syscall #38 (SYS_sched_algorithm)
  - algo=-1: 查询当前算法
  - algo=0~5: 切换算法 (RR/FCFS/MLFQ/SJF/PRIO/EDF)
  - 返回值: 切换前的算法编号，失败返回 -1
- `sched_algorithm_name(int algo)` — 用户态辅助函数，定义在 `user/sched.c`
