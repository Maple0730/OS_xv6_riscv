// Scheduling Latency Test Program
// Measures the time between a process becoming RUNNABLE
// and actually getting CPU time (start to first run)

#include "kernel/types.h"
#include "user/user.h"

#define NUM_WORKERS 8

void worker(int id, int delay) {
  int start = uptime();
  volatile long sum = 0;
  // Small amount of work
  for (int i = 0; i < 50000; i++) {
    sum += i;
  }
  int elapsed = uptime() - start;
  printf("  Worker %d: started, ran for %d ticks\n", id, elapsed);
  exit(0);
}

int main(int argc, char *argv[]) {
  int start_time = uptime();
  
  printf("=== Scheduling Latency Test ===\n");
  printf("Creating %d workers at staggered times\n\n", NUM_WORKERS);
  
  for (int i = 0; i < NUM_WORKERS; i++) {
    int pid = fork();
    if (pid == 0) {
      worker(i, i * 5);
      exit(0);
    }
    // Stagger creation by 5 ticks
    if (i < NUM_WORKERS - 1)
      pause(5);
  }
  
  printf("\nWaiting for all workers...\n");
  for (int i = 0; i < NUM_WORKERS; i++) {
    wait(0);
  }
  
  int total = uptime() - start_time;
  printf("\nTotal time: %d ticks\n", total);
  printf("Average time per worker: %d ticks\n", total / NUM_WORKERS);
  
  return 0;
}
