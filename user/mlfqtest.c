// MLFQ 调度算法测试程序
// 测试目的：验证 MLFQ 调度器的优先级机制
// 1. 短作业应该获得更好的响应时间
// 2. CPU 密集型作业应该被降级到低优先级队列
// 3. 长时间等待的进程应该被提升优先级
//
// 优化：增加工作负载差异，让优先级效果更明显

#include "kernel/types.h"
#include "user/user.h"

#define NUM_SHORT 3    // 短作业数量
#define NUM_LONG  2    // 长作业数量
#define NUM_MIXED 2   // 混合作业数量

// 短作业：执行少量 CPU 工作后退出
void short_job(int id) {
  int pid = getpid();
  int start = uptime();

  printf("  [SHORT] Job %d (PID=%d) started at tick %d\n", id, pid, start);

  // 少量计算工作 - 应该在高优先级队列完成
  volatile long sum = 0;
  for (int i = 0; i < 150000; i++) {  // 增加工作量
    sum += i;
  }

  int elapsed = uptime() - start;
  printf("  [SHORT] Job %d (PID=%d) finished at tick %d (elapsed=%d)\n",
         id, pid, uptime(), elapsed);

  exit(0);
}

// 长作业：执行大量 CPU 工作
void long_job(int id) {
  int pid = getpid();
  int start = uptime();

  printf("  [LONG] Job %d (PID=%d) started at tick %d\n", id, pid, start);

  // 大量计算工作 - 会用完时间片并被降级
  volatile long sum = 0;
  for (int i = 0; i < 1500000; i++) {  // 大幅增加工作量
    sum += i;
  }

  int elapsed = uptime() - start;
  printf("  [LONG] Job %d (PID=%d) finished at tick %d (elapsed=%d)\n",
         id, pid, uptime(), elapsed);

  exit(0);
}

// 混合作业：交替执行 CPU 工作和暂停
void mixed_job(int id) {
  int pid = getpid();
  int start = uptime();

  printf("  [MIXED] Job %d (PID=%d) started at tick %d\n", id, pid, start);

  // 多轮 I/O 交互
  for (int round = 0; round < 8; round++) {  // 增加轮数
    // CPU 工作
    volatile long sum = 0;
    for (int i = 0; i < 120000; i++) {
      sum += i;
    }
    // 暂停模拟 I/O
    pause(1);
  }

  int elapsed = uptime() - start;
  printf("  [MIXED] Job %d (PID=%d) finished at tick %d (elapsed=%d)\n",
         id, pid, uptime(), elapsed);

  exit(0);
}

int main(int argc, char *argv[]) {
  int pid;
  int start_time = uptime();

  printf("=== MLFQ Scheduler Test ===\n");
  printf("Parent PID: %d, started at tick %d\n\n", getpid(), start_time);
  printf("Workload: %d SHORT, %d LONG, %d MIXED jobs\n\n",
         NUM_SHORT, NUM_LONG, NUM_MIXED);

  // 创建短作业
  printf("Creating %d SHORT jobs...\n", NUM_SHORT);
  for (int i = 0; i < NUM_SHORT; i++) {
    pid = fork();
    if (pid == 0) {
      short_job(i);
      exit(0);
    }
    if (pid < 0) {
      printf("Fork failed!\n");
      exit(1);
    }
  }

  // 创建长作业
  printf("Creating %d LONG jobs...\n", NUM_LONG);
  for (int i = 0; i < NUM_LONG; i++) {
    pid = fork();
    if (pid == 0) {
      long_job(i);
      exit(0);
    }
    if (pid < 0) {
      printf("Fork failed!\n");
      exit(1);
    }
  }

  // 创建混合作业
  printf("Creating %d MIXED jobs...\n", NUM_MIXED);
  for (int i = 0; i < NUM_MIXED; i++) {
    pid = fork();
    if (pid == 0) {
      mixed_job(i);
      exit(0);
    }
    if (pid < 0) {
      printf("Fork failed!\n");
      exit(1);
    }
  }

  // 父进程等待所有子进程
  printf("\nParent waiting for all children...\n");
  int total = NUM_SHORT + NUM_LONG + NUM_MIXED;

  for (int i = 0; i < total; i++) {
    int exit_pid = wait(0);
    int current_time = uptime();
    printf("  Child PID=%d exited at tick %d\n", exit_pid, current_time);
  }

  int end_time = uptime();
  printf("\n=== MLFQ Test Results ===\n");
  printf("Total time: %d ticks\n", end_time - start_time);

  // 分析结果
  printf("\n--- Analysis ---\n");
  printf("Expected MLFQ behavior:\n");
  printf("  1. SHORT jobs: Should complete fastest (high priority queue)\n");
  printf("  2. LONG jobs: May take longer (demoted to lower queue)\n");
  printf("  3. MIXED jobs: Moderate time (alternating CPU/I/O)\n");
  printf("\nPriority queue timeslices:\n");
  printf("  Queue 0 (highest): 5ms\n");
  printf("  Queue 1 (medium): 10ms\n");
  printf("  Queue 2 (lowest): 20ms\n");

  return 0;
}
