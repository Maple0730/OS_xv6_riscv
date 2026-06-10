// 吞吐量测试程序
// 测试目的：对比不同调度算法的整体吞吐量
// 方法：运行相同工作负载，测量完成时间

#include "kernel/types.h"
#include "user/user.h"

#define NUM_PROCS 8
#define WORK_PER_PROC 20000

void worker(int id) {
  volatile long sum = 0;
  // 执行计算工作
  for (int i = 0; i < WORK_PER_PROC * 1000; i++) {
    sum += i;
  }
  exit(0);
}

int main(int argc, char *argv[]) {
  int start_time = uptime();
  int pids[NUM_PROCS];

  printf("=== Throughput Test ===\n");
  printf("Running %d processes with %d iterations each\n", NUM_PROCS, WORK_PER_PROC * 100);

  // 创建进程
  for (int i = 0; i < NUM_PROCS; i++) {
    pids[i] = fork();
    if (pids[i] == 0) {
      worker(i);
      exit(0);  // never reached
    }
  }

  // 等待所有进程
  for (int i = 0; i < NUM_PROCS; i++) {
    wait(0);
  }

  int end_time = uptime();
  int total_time = end_time - start_time;

  printf("\n=== Results ===\n");
  printf("Total time: %d ticks\n", total_time);
  printf("Average per process: ~%d ticks\n", total_time / NUM_PROCS);
  printf("Ticks per iteration: ~%d\n", total_time * 1000 / (NUM_PROCS * WORK_PER_PROC));

  return 0;
}
