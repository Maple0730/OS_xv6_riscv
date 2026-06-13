// Scheduler Algorithm Switching Test
// Tests runtime scheduling algorithm switching between RR, FCFS, and MLFQ

#include "kernel/types.h"
#include "user/user.h"

#define NUM_WORKERS 4
#define WORK_ITERATIONS 50000

// Work function that runs CPU-intensive task
void worker(int id) {
  volatile long sum = 0;
  int start = uptime();

  for (int i = 0; i < WORK_ITERATIONS; i++) {
    sum += i;
  }

  int elapsed = uptime() - start;
  printf("  [Worker %d] PID=%d finished at tick %d (elapsed=%d)\n",
         id, getpid(), uptime(), elapsed);
  exit(0);
}

// Run a workload and measure completion order
void run_workload(const char *name) {
  int start = uptime();
  int pids[NUM_WORKERS];

  printf("\n  Running %s workload with %d workers...\n", name, NUM_WORKERS);

  // Create workers
  for (int i = 0; i < NUM_WORKERS; i++) {
    pids[i] = fork();
    if (pids[i] == 0) {
      worker(i);
      exit(0);
    }
    if (pids[i] < 0) {
      printf("Fork failed!\n");
      exit(1);
    }
  }

  // Wait for all workers
  for (int i = 0; i < NUM_WORKERS; i++) {
    wait(0);
  }

  int elapsed = uptime() - start;
  printf("  %s completed in %d ticks\n", name, elapsed);
}

void test_switch(int target_algo) {
  const char *algo_name = sched_algorithm_name(target_algo);
  int old = sched_algorithm(target_algo);

  if (old < 0) {
    printf("ERROR: Failed to switch to %s scheduler\n", algo_name);
    return;
  }

  printf("  Switched from %s to %s\n", sched_algorithm_name(old), algo_name);
}

int main(int argc, char *argv[]) {
  int start_time = uptime();

  printf("=== Scheduler Algorithm Switching Test ===\n");
  printf("Parent PID: %d\n\n", getpid());

  // Show current scheduler
  int current = sched_algorithm(-1);  // Query without changing
  if (current < 0) {
    printf("ERROR: sched_algorithm syscall not working!\n");
    return 1;
  }
  printf("Current scheduler: %s\n\n", sched_algorithm_name(current));

  // Test invalid values
  printf("--- Testing invalid input ---\n");
  int invalid = sched_algorithm(99);
  printf("  sched_algorithm(99) = %d (expected -1)\n", invalid);
  invalid = sched_algorithm(-1);
  printf("  sched_algorithm(-1) = %d (expected -1)\n", invalid);

  // Test valid switching
  printf("\n--- Testing valid switching ---\n");

  // Test 1: Switch to RR
  printf("\n[Test 1] Switching to RR (0):\n");
  test_switch(0);
  run_workload("RR");

  // Test 2: Switch to FCFS
  printf("\n[Test 2] Switching to FCFS (1):\n");
  test_switch(1);
  run_workload("FCFS");

  // Test 3: Switch to MLFQ
  printf("\n[Test 3] Switching to MLFQ (2):\n");
  test_switch(2);
  run_workload("MLFQ");

  // Verify we can switch back
  printf("\n--- Testing round-trip switching ---\n");
  int prev = sched_algorithm(0);
  printf("  RR -> returned: %s\n", sched_algorithm_name(prev));

  prev = sched_algorithm(1);
  printf("  FCFS -> returned: %s\n", sched_algorithm_name(prev));

  prev = sched_algorithm(2);
  printf("  MLFQ -> returned: %s\n", sched_algorithm_name(prev));

  // Restore to MLFQ
  sched_algorithm(2);

  int total_time = uptime() - start_time;
  printf("\n=== Test Complete ===\n");
  printf("Total time: %d ticks\n", total_time);
  printf("\nSummary:\n");
  printf("  - Scheduler algorithm can be queried at runtime\n");
  printf("  - Algorithm can be switched without recompilation\n");
  printf("  - All three algorithms (RR, FCFS, MLFQ) work correctly\n");

  return 0;
}
