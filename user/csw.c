// 上下文切换开销测试程序
// 测试目的：测量进程切换的平均开销
// 方法：创建多个进程循环执行 pause()，观察系统响应

#include "kernel/types.h"
#include "user/user.h"

#define NUM_CHILDREN 4
#define PAUSE_DURATION 1

void pause_child(int id) {
  int pid = getpid();
  int start = uptime();

  printf("  Pause child %d (PID=%d) starting\n", id, pid);

  // 执行大量 pause，每次 pause 都会导致进程切换
  for (int i = 0; i < 50; i++) {
    pause(PAUSE_DURATION);
  }

  int end = uptime();

  printf("  Pause child %d (PID=%d) finished in %d ticks\n", id, pid, end - start);

  exit(0);
}

int main(int argc, char *argv[]) {
  int start_time = uptime();

  printf("=== Context Switch Overhead Test ===\n");
  printf("Parent PID: %d\n", getpid());
  printf("Testing with %d children, each doing %d pauses\n\n",
         NUM_CHILDREN, 50);

  // 创建子进程
  for (int i = 0; i < NUM_CHILDREN; i++) {
    int pid = fork();
    if (pid == 0) {
      pause_child(i);
      exit(0);
    }
    if (pid < 0) {
      printf("Fork failed!\n");
      exit(1);
    }
  }

  // 父进程也执行一些工作
  printf("Parent doing some work...\n");
  volatile int sum = 0;
  for (int i = 0; i < 100000; i++) {
    sum += i;
  }

  // 等待所有子进程
  printf("\nWaiting for children...\n");
  for (int i = 0; i < NUM_CHILDREN; i++) {
    wait(0);
  }

  int end_time = uptime();

  printf("\n=== Results ===\n");
  printf("Total time: %d ticks\n", end_time - start_time);
  printf("Children completed their work while parent was running.\n");

  printf("\n--- Analysis ---\n");
  printf("Context switch overhead includes:\n");
  printf("  - Timer interrupt handling\n");
  printf("  - Scheduler invocation\n");
  printf("  - Context save/restore (swtch)\n");
  printf("  - Process selection\n");

  return 0;
}
