// SJF test: all children are RUNNABLE at the same time
// Strategy: parent forks all children, each child sets burst and busy-waits
// The key insight: the parent's forking loop is fast enough that all children
// are created before any of them finishes their busy-wait

#include "kernel/types.h"
#include "user/user.h"

#define N 4

int
main(int argc, char *argv[])
{
  int pid;
  int start = uptime();

  printf("=== SJF Test (busy-wait approach) ===\n");
  printf("Switching to SJF...\n");
  sched_algorithm(3);

  // est_burst and actual work (in millions of iterations)
  // child 0: est=8, work=8M  (longest)
  // child 1: est=4, work=4M
  // child 2: est=2, work=2M
  // child 3: est=1, work=1M  (shortest)
  int est[N] = {8, 4, 2, 1};
  int work_millions[N] = {8, 4, 2, 1};

  for (int i = 0; i < N; i++) {
    pid = fork();
    if (pid == 0) {
      // Child: set burst first, then busy-wait
      sched_setburst(getpid(), est[i]);
      int t0 = uptime();
      volatile long sum = 0;
      for (int j = 0; j < work_millions[i] * 1000000; j++) {
        sum += j;
      }
      int t1 = uptime();
      printf("[child %d] PID=%d est=%d work=%dM FINISHED at tick %d (took %d ticks)\n",
             i, getpid(), est[i], work_millions[i], t1, t1 - t0);
      exit(0);
    }
    // NO delay between forks - let all children be created quickly
  }

  // Wait for all children
  for (int i = 0; i < N; i++) {
    wait(0);
  }

  int end = uptime();
  printf("Total: %d ticks\n", end - start);
  printf("Expected SJF order: child 3 (est=1) -> child 2 (est=2) -> child 1 (est=4) -> child 0 (est=8)\n");

  sched_algorithm(2);
  return 0;
}
