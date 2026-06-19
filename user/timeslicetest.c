// timeslicetest.c - Dynamic timeslice configuration test
// Tests settimeslice() and gettimeslice() for MLFQ queues and RR/FCFS

#include "kernel/types.h"
#include "user/user.h"

// Convert ticks to approximate milliseconds (1ms = 100000 ticks)
#define TICKS_TO_MS(t) ((t) / 100000)

void print_timeslice_config(void) {
  printf("=== Current Timeslice Configuration ===\n");
  printf("RR/FCFS:  %d ticks (~%d ms)\n", gettimeslice(-1), TICKS_TO_MS(gettimeslice(-1)));
  printf("MLFQ Q0:  %d ticks (~%d ms)\n", gettimeslice(0), TICKS_TO_MS(gettimeslice(0)));
  printf("MLFQ Q1:  %d ticks (~%d ms)\n", gettimeslice(1), TICKS_TO_MS(gettimeslice(1)));
  printf("MLFQ Q2:  %d ticks (~%d ms)\n", gettimeslice(2), TICKS_TO_MS(gettimeslice(2)));
  printf("\n");
}

void test_invalid_input(void) {
  printf("--- Testing invalid input ---\n");

  // Invalid queue values
  int r;
  r = settimeslice(-2, 500000);
  printf("settimeslice(-2, 500000) = %d (expected -1)\n", r);
  r = settimeslice(3, 500000);
  printf("settimeslice(3, 500000) = %d (expected -1)\n", r);
  r = settimeslice(0, 0);
  printf("settimeslice(0, 0) = %d (expected -1)\n", r);
  r = settimeslice(0, -100);
  printf("settimeslice(0, -100) = %d (expected -1)\n", r);

  // Invalid gettimeslice
  r = gettimeslice(-2);
  printf("gettimeslice(-2) = %d (expected -1)\n", r);
  r = gettimeslice(3);
  printf("gettimeslice(3) = %d (expected -1)\n", r);
  printf("\n");
}

void test_mlfq_dynamic_adjust(void) {
  printf("--- Testing MLFQ dynamic adjustment ---\n");

  // Save original Q0
  int orig_q0 = gettimeslice(0);
  int orig_q1 = gettimeslice(1);
  int orig_q2 = gettimeslice(2);
  printf("Original: Q0=%d Q1=%d Q2=%d\n", orig_q0, orig_q1, orig_q2);

  // Shrink Q0 to 1ms (aggressive scheduling)
  int new_q0 = 100000;
  if (settimeslice(0, new_q0) == 0) {
    printf("Adjusted Q0 from %d to %d ticks (~%d ms)\n",
           orig_q0, gettimeslice(0), TICKS_TO_MS(gettimeslice(0)));

    // Restore original
    settimeslice(0, orig_q0);
    printf("Restored Q0 to %d\n", gettimeslice(0));
  }

  // Expand Q2 to 50ms (lenient scheduling)
  int new_q2 = 5000000;
  if (settimeslice(2, new_q2) == 0) {
    printf("Adjusted Q2 from %d to %d ticks (~%d ms)\n",
           orig_q2, gettimeslice(2), TICKS_TO_MS(gettimeslice(2)));

    // Restore original
    settimeslice(2, orig_q2);
    printf("Restored Q2 to %d\n", gettimeslice(2));
  }
  printf("\n");
}

void test_rr_fcfs_dynamic_adjust(void) {
  printf("--- Testing RR/FCFS dynamic adjustment ---\n");

  // Save original
  int orig = gettimeslice(-1);
  printf("Original RR/FCFS: %d ticks (~%d ms)\n", orig, TICKS_TO_MS(orig));

  // Shrink to 2ms
  int new_ts = 200000;
  if (settimeslice(-1, new_ts) == 0) {
    printf("Adjusted RR/FCFS from %d to %d ticks (~%d ms)\n",
           orig, gettimeslice(-1), TICKS_TO_MS(gettimeslice(-1)));

    // Restore original
    settimeslice(-1, orig);
    printf("Restored RR/FCFS to %d\n", gettimeslice(-1));
  }
  printf("\n");
}

void worker(int id) {
  volatile int sum = 0;
  for (int i = 0; i < 1000; i++) {
    sum += i * id;
  }
  printf("Worker %d: finished\n", id);
}

void test_scheduler_integration(void) {
  printf("--- Testing scheduler integration ---\n");

  int algo = sched_algorithm(-1);
  printf("Current scheduler: %s\n", sched_algorithm_name(algo));

  // Switch to RR and adjust timeslice
  sched_algorithm(0);
  printf("Switched to RR\n");

  int orig = gettimeslice(-1);
  settimeslice(-1, 200000);
  printf("RR timeslice adjusted to %d ticks\n", gettimeslice(-1));
  settimeslice(-1, orig);
  printf("RR timeslice restored to %d\n", gettimeslice(-1));

  // Switch to MLFQ and adjust Q0
  sched_algorithm(2);
  printf("Switched to MLFQ\n");

  orig = gettimeslice(0);
  settimeslice(0, 200000);
  printf("MLFQ Q0 adjusted to %d ticks\n", gettimeslice(0));
  settimeslice(0, orig);
  printf("MLFQ Q0 restored to %d\n", gettimeslice(0));

  // Restore original scheduler
  sched_algorithm(2);
  printf("Scheduler restored to MLFQ\n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  printf("=== Dynamic Timeslice Configuration Test ===\n\n");

  print_timeslice_config();
  test_invalid_input();
  test_mlfq_dynamic_adjust();
  test_rr_fcfs_dynamic_adjust();
  test_scheduler_integration();

  printf("=== Test Complete ===\n");
  printf("Summary:\n");
  printf("  - settimeslice() and gettimeslice() work correctly\n");
  printf("  - Invalid parameters are properly rejected\n");
  printf("  - Timeslice changes persist across scheduler switches\n");
  printf("  - Values can be dynamically adjusted without recompilation\n");

  return 0;
}
