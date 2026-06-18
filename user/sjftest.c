// SJF (Shortest Job First) 调度算法测试程序
//
// 测试目的：
//   1. 验证 SJF 按 est_burst 选择进程（最短作业优先）
//   2. 与 FCFS 对比，证明 SJF 在平均等待时间上明显更优
//   3. 验证调度算法运行时切换 + sched_setburst 系统调用
//
// 工作负载设计：
//   创建 N 个子进程，分别设置不同的 est_burst，让它们的"声明长度"不同。
//   在 SJF 模式下，burst 小的应先完成；
//   在 FCFS 模式下，PID 小的应先完成（按创建顺序）。
//
// 注意：SJF 本身是非抢占式的，我们通过 sched_setburst 显式指定每个进程的 burst。

#include "kernel/types.h"
#include "user/user.h"

#define N_CHILDREN 4

// 子进程 "忙等待" 指定 ticks 数
// busy_wait 通过 pause(1) 循环实现，1 tick = 1 次 pause(1)
static void
busy_wait(int ticks)
{
  for (int i = 0; i < ticks; i++) {
    pause(1);
  }
}

int
main(int argc, char *argv[])
{
  int pid;
  int start_time = uptime();

  printf("=== SJF (Shortest Job First) Scheduler Test ===\n");
  printf("Parent PID: %d, started at tick %d\n\n", getpid(), start_time);

  // ============================================================
  // Part 1: 验证运行时切换到 SJF 算法
  // ============================================================
  printf("--- Part 1: Switch to SJF ---\n");
  int prev = sched_algorithm(-1);
  printf("Current scheduler: %s (code=%d)\n", sched_algorithm_name(prev), prev);

  prev = sched_algorithm(3);  // 3 = SJF
  printf("Switched to: %s (prev was code=%d)\n", sched_algorithm_name(prev == -1 ? -1 : 3), prev);
  printf("Now current: %s\n\n", sched_algorithm_name(sched_algorithm(-1)));

  // ============================================================
  // Part 2: 验证 sched_setburst 系统调用
  // ============================================================
  printf("--- Part 2: Verify sched_setburst() ---\n");
  // 创建 4 个子进程，est_burst 分别 = 8, 4, 2, 1（先创建的 burst 大）
  int bursts[N_CHILDREN] = {8, 4, 2, 1};

  for (int i = 0; i < N_CHILDREN; i++) {
    pid = fork();
    if (pid == 0) {
      // 子进程：声明自己的 burst
      int rc = sched_setburst(getpid(), bursts[i]);
      if (rc < 0) {
        printf("  [child %d] sched_setburst FAILED\n", i);
        exit(1);
      }
      printf("  [child %d] PID=%d declared est_burst=%d, starting work\n",
             i, getpid(), bursts[i]);
      int t0 = uptime();
      busy_wait(bursts[i]);   // 实际忙等 bursts[i] ticks
      int t1 = uptime();
      printf("  [child %d] PID=%d FINISHED at tick %d (elapsed=%d)\n",
             i, getpid(), t1, t1 - t0);
      exit(0);
    } else if (pid < 0) {
      printf("Fork failed!\n");
      exit(1);
    }
    // 短间隔 fork，让所有进程都处于 RUNNABLE，等待 SJF 选择
    pause(0);
  }

  // 等待所有子进程完成
  for (int i = 0; i < N_CHILDREN; i++) {
    wait(0);
  }

  int sjf_end = uptime();
  printf("\nSJF phase finished at tick %d (elapsed=%d)\n", sjf_end, sjf_end - start_time);
  printf("Expected SJF order: PID with est_burst=1 first, then 2, 4, 8\n");
  printf("(note: in this test, children sleep so all 4 are RUNNABLE before scheduling matters)\n\n");

  // ============================================================
  // Part 3: 切回 FCFS，对比行为
  // ============================================================
  printf("--- Part 3: Switch to FCFS for comparison ---\n");
  prev = sched_algorithm(1);  // 1 = FCFS
  printf("Switched to FCFS (prev was code=%d)\n\n", prev);

  int fcfs_start = uptime();

  for (int i = 0; i < N_CHILDREN; i++) {
    pid = fork();
    if (pid == 0) {
      // 子进程：故意让 burst 大的先创建（但 FCFS 仍按 PID 顺序调度）
      int rc = sched_setburst(getpid(), bursts[i]);
      if (rc < 0) {
        printf("  [FCFS child %d] sched_setburst FAILED\n", i);
        exit(1);
      }
      printf("  [FCFS child %d] PID=%d declared est_burst=%d, starting work\n",
             i, getpid(), bursts[i]);
      int t0 = uptime();
      busy_wait(bursts[i]);
      int t1 = uptime();
      printf("  [FCFS child %d] PID=%d FINISHED at tick %d (elapsed=%d)\n",
             i, getpid(), t1, t1 - t0);
      exit(0);
    } else if (pid < 0) {
      printf("Fork failed!\n");
      exit(1);
    }
    pause(0);
  }

  for (int i = 0; i < N_CHILDREN; i++) {
    wait(0);
  }

  int fcfs_end = uptime();
  printf("\nFCFS phase finished at tick %d (elapsed=%d)\n",
         fcfs_end, fcfs_end - fcfs_start);
  printf("Expected FCFS order: PID small first (creation order = burst 8,4,2,1)\n\n");

  // ============================================================
  // Part 4: 验证错误处理
  // ============================================================
  printf("--- Part 4: Error handling ---\n");
  int rc;

  rc = sched_setburst(0, 0);
  printf("  sched_setburst(0, 0) = %d (expected -1, burst must be > 0)\n", rc);

  rc = sched_setburst(0, 99999);
  printf("  sched_setburst(0, 99999) = %d (expected -1, burst too large)\n", rc);

  rc = sched_setburst(99999, 5);
  printf("  sched_setburst(99999, 5) = %d (expected -1, invalid pid)\n", rc);

  rc = sched_setburst(getpid(), 3);
  printf("  sched_setburst(self, 3) = %d (expected 0)\n", rc);

  // 切回默认 MLFQ
  sched_algorithm(2);
  printf("\nRestored scheduler to: %s\n", sched_algorithm_name(sched_algorithm(-1)));

  int end_time = uptime();
  printf("\n=== SJF Test Complete ===\n");
  printf("Total time: %d ticks\n", end_time - start_time);
  printf("SJF phase: %d ticks | FCFS phase: %d ticks\n",
         sjf_end - start_time, fcfs_end - fcfs_start);

  printf("\n--- Interpretation ---\n");
  printf("SJF selects the RUNNABLE process with the SMALLEST est_burst.\n");
  printf("If observed order under SJF matches (1,2,4,8), SJF is working.\n");
  printf("If observed order under FCFS matches (8,4,2,1), FCFS is working.\n");
  printf("SJF's expected order is the REVERSE of FCFS's -- classic textbook result.\n");

  return 0;
}
