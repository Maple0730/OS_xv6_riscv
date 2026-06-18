// Schedstat Test Program
// Reads scheduling statistics for running processes

#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  struct sched_stat s;
  
  printf("=== Scheduling Statistics ===\n");
  printf("Current process (PID=%d):\n", getpid());
  
  if (schedstat(0, &s) == 0) {
    printf("  queue_level=%d, sched_count=%d\n", s.queue_level, s.sched_count);
    printf("  wait_time=%lu, run_time=%lu\n", s.wait_time, s.run_time);
  } else {
    printf("  FAILED to get stats\n");
  }
  
  // Show stats for all PIDs 1-20
  printf("\nScanning PIDs 1-20 for scheduling stats:\n");
  for (int pid = 1; pid <= 20; pid++) {
    if (schedstat(pid, &s) == 0) {
      printf("  PID %d: queue=%d sched_count=%d wait=%lu run=%lu\n",
             s.pid, s.queue_level, s.sched_count, s.wait_time, s.run_time);
    }
  }
  
  return 0;
}
