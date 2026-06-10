// FCFS 调度算法测试程序
// 测试目的：验证 FCFS 按进程创建顺序（PID 顺序）调度
// 预期行为：先创建的进程（PID 小）应该先完成
//
// 优化：增加工作负载，让每个进程运行更长时间

#include "kernel/types.h"
#include "user/user.h"

#define NUM_CHILDREN 4
#define WORK_ITERATIONS 20000

void child_work(int id, int iterations) {
  int pid = getpid();
  int start_time = uptime();

  printf("Child %d (PID=%d) started at tick %d\n", id, pid, start_time);

  volatile long sum = 0;
  for (int i = 0; i < iterations * 1000; i++) {
    sum += i;
    if (i % 10000 == 0) {
      sum = sum * 3 + i;
    }
  }

  int end_time = uptime();
  int elapsed = end_time - start_time;

  printf("Child %d (PID=%d) finished at tick %d (took %d ticks)\n",
         id, pid, end_time, elapsed);

  exit(0);
}

int main(int argc, char *argv[]) {
  int pid;
  int start_time = uptime();

  printf("=== FCFS Scheduler Test ===\n");
  printf("Parent PID: %d, started at tick %d\n", getpid(), start_time);
  printf("Workload: %d children, %d iterations each\n", NUM_CHILDREN, WORK_ITERATIONS);
  printf("Expected: PID order should match completion order\n\n");

  for (int i = 0; i < NUM_CHILDREN; i++) {
    pid = fork();
    if (pid == 0) {
      child_work(i, WORK_ITERATIONS);
      exit(0);
    } else if (pid < 0) {
      printf("Fork failed!\n");
      exit(1);
    }
    if (i < NUM_CHILDREN - 1) {
      pause(1);
    }
  }

  printf("\nParent waiting for children...\n");

  int exit_pid;
  int first_pid = -1, last_pid = -1;

  for (int i = 0; i < NUM_CHILDREN; i++) {
    exit_pid = wait(0);
    if (first_pid == -1)
      first_pid = exit_pid;
    last_pid = exit_pid;
    printf("  -> Child PID=%d exited\n", exit_pid);
  }

  int end_time = uptime();

  printf("\n=== FCFS Test Results ===\n");
  printf("Total time: %d ticks\n", end_time - start_time);
  printf("First exited: PID=%d, Last exited: PID=%d\n", first_pid, last_pid);

  printf("\n--- Verification ---\n");
  printf("If exit order follows PID order, FCFS behavior is supported.\n");

  printf("\n--- Timing Analysis ---\n");
  printf("Workload per child: %d * 1000 = %d iterations\n", WORK_ITERATIONS, WORK_ITERATIONS * 1000);

  return 0;
}
