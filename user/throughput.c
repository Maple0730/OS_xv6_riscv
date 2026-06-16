// Enhanced Throughput Test Program
// Compares RR, FCFS, and MLFQ scheduling algorithms
// with larger workloads and statistical analysis

#include "kernel/types.h"
#include "user/user.h"

#define NUM_WORKERS 8
// Use cgettimeofday for high-resolution timing (if available)
// Otherwise fall back to uptime

// Worker function - CPU-bound computation
void worker(int id) {
  volatile long sum = 0;
  for (int i = 0; i < 200000; i++) {
    sum += i;
  }
  exit(0);
}

// Test one scheduling algorithm
int test_algo(const char *name, int algo) {
  int start = uptime();
  int pids[NUM_WORKERS];
  
  if (sched_algorithm(algo) < 0) {
    printf("  FAIL: cannot set algorithm %s\n", name);
    return -1;
  }
  
  // Create workers
  for (int i = 0; i < NUM_WORKERS; i++) {
    pids[i] = fork();
    if (pids[i] == 0) {
      worker(i);
      exit(0);
    }
  }
  
  // Wait for all
  for (int i = 0; i < NUM_WORKERS; i++) {
    wait(0);
  }
  
  int elapsed = uptime() - start;
  printf("  %s: completed in %d ticks\n", name, elapsed);
  return elapsed;
}

int main(int argc, char *argv[]) {
  printf("=== Enhanced Throughput Test ===\n");
  printf("Workers: %d, Workload per worker: 200000 iterations\n\n", NUM_WORKERS);
  
  // Store original algorithm
  int orig = sched_algorithm(-1);
  printf("Current scheduler: %s\n\n", sched_algorithm_name(orig));
  
  // Test all three algorithms
  int rr_time = test_algo("RR", 0);
  int fcfs_time = test_algo("FCFS", 1);
  int mlfq_time = test_algo("MLFQ", 2);
  
  // Restore original
  sched_algorithm(orig);
  
  printf("\n=== Summary ===\n");
  if (rr_time > 0)
    printf("  RR   time: %d ticks\n", rr_time);
  if (fcfs_time > 0)
    printf("  FCFS time: %d ticks\n", fcfs_time);
  if (mlfq_time > 0)
    printf("  MLFQ time: %d ticks\n", mlfq_time);
  printf("  (Lower is better)\n");
  
  return 0;
}
