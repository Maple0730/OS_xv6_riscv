#define NPROC       64                // maximum number of processes
#define NCPU        8                 // maximum number of CPUs
#define NOFILE      16                // open files per process
#define NFILE       100               // open files per system 全局打开文件表上限
#define NINODE      50                // maximum number of active i-nodes
#define NDEV        10                // maximum major device number
#define ROOTDEV     1                 // device number of file system root disk
#define MAXARG      32                // max exec arguments
#define MAXOPBLOCKS 10                // max # of blocks any FS op writes
#define LOGBLOCKS   (MAXOPBLOCKS * 3) // max data blocks in on-disk log
#define NBUF        (MAXOPBLOCKS * 3) // size of disk block cache
#define FSSIZE      4000              // size of file system in blocks (2MB; up from 2000 to accommodate more UPROGS)
#define MAXPATH     128               // maximum file path name
#define USERSTACK   1                 // user stack pages

// ============================================
// 调度算法配置
// ============================================
// 调度算法选择: 0=RR, 1=FCFS, 2=MLFQ, 3=SJF, 4=PRIO, 5=EDF
#define SCHED_RR      0  // 时间片轮转（默认）
#define SCHED_FCFS    1  // 先来先服务
#define SCHED_MLFQ    2  // 多级反馈队列
#define SCHED_SJF     3  // 短作业优先（非抢占式，按估算 burst 选择）
#define SCHED_PRIO    4  // 静态优先级 + aging
#define SCHED_EDF     5  // 最早截止时间优先 (F2)

// 默认调度算法
#ifndef SCHED_ALGORITHM
#define SCHED_ALGORITHM SCHED_MLFQ
#endif

// MLFQ 调度参数（单位：tick ≈ 10ms）
#define MLFQ_LEVELS      5       // 队列层数 (0=最高, 1=2, 2=3, 3=4, 4=最低)
#define MLFQ_Q0_TIME     1       // Queue 0 时间片（1 tick ≈ 10ms, 最高优先级）
#define MLFQ_Q1_TIME     2       // Queue 1 时间片（2 ticks ≈ 20ms）
#define MLFQ_Q2_TIME     4       // Queue 2 时间片（4 ticks ≈ 40ms）
#define MLFQ_Q3_TIME     8       // Queue 3 时间片（8 ticks ≈ 80ms）
#define MLFQ_Q4_TIME     15      // Queue 4 时间片（15 ticks ≈ 150ms, 最低优先级）
#define MLFQ_BOOST_TICKS 100     // 每 100 ticks 提升一次优先级（防止饥饿）

// MLFQ 调试开关：设置为 1 启用 [MLFQ] 日志输出，设置为 0 关闭
#ifndef MLFQ_DEBUG
#define MLFQ_DEBUG 0
#endif

// FCFS/RR 共用时间片（默认10ms）
#ifndef TICKSLICE
#define TICKSLICE 1000000  // 1000000 ticks ≈ 10ms
#endif

// 优先级配置（用于 FCFS 和 MLFQ）
#define MAX_PRIORITY 10     // 最大优先级（数值越小优先级越高）
#define DEFAULT_PRIORITY 5 // 默认优先级

// SJF 调度参数
#define SJF_DEFAULT_BURST 5  // 默认估算 CPU 突发时间（ticks）
#define SJF_MAX_BURST     1000 // 用户可设置的最大 burst 值

// 优先级调度参数（Phase A2）
#define PRIO_AGING_TICKS   50   // 每 50 ticks 触发一次 aging（提升 RUNNABLE 进程的优先级）
#define PRIO_AGING_DEC     1    // 每次 aging 减 1（数值越小优先级越高，0=最高）
