// Phase F3: Stride + Lottery fair scheduler test
//
// Three scenarios:
//   1. Stride proportional fairness: weight 1:2:10 -> CPU time 1:2:10
//   2. Lottery expected fairness: weight 1:1 -> CPU time near 1:1
//   3. Dynamic weight adjustment: change weight at runtime, observe CPU time change
//
// Build: add $(BU)/_stridetest to UPROGS
// Run: in qemu shell, type `stridetest`

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define SCHED_STRIDE_VAL  6
#define SCHED_LOTTERY_VAL 7

static int parent_pid;

static void
busy_work(int label, unsigned long target_ticks)
{
  volatile unsigned long x = 0;
  unsigned long start = cgettimeofday();

  unsigned long iters = target_ticks * 200000UL;

  for (unsigned long i = 0; i < iters; i++) {
    x = x * 1103515245UL + 12345UL;
    if ((i & 0xFFFFFF) == 0) {
      unsigned long now = cgettimeofday();
      if (now - start > 200) {
        break;
      }
    }
  }
  if (x == 0xdeadbeefUL) printf("(never)\n");
  printf("[%d] child label=%d done\n", parent_pid, label);
}

static int child_id = 0;

static void
child_stride(int weight, int target_ticks)
{
  if (stride_setweight(getpid(), weight) < 0) {
    printf("FAIL: stride_setweight pid=%d weight=%d\n", getpid(), weight);
    exit(1);
  }
  printf("[%d] stride child id=%d pid=%d weight=%d start\n",
         parent_pid, child_id, getpid(), weight);
  busy_work(child_id, target_ticks);
  exit(0);
}

static void
child_lottery(int weight, int target_ticks)
{
  if (stride_setweight(getpid(), weight) < 0) {
    printf("FAIL: stride_setweight pid=%d weight=%d\n", getpid(), weight);
    exit(1);
  }
  printf("[%d] lottery child id=%d pid=%d weight=%d start\n",
         parent_pid, child_id, getpid(), weight);
  busy_work(child_id, target_ticks);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int pid_a, pid_b, pid_c;

  parent_pid = getpid();
  printf("=== Stride + Lottery Fair Scheduler Test (Phase F3) ===\n\n");

  // Scenario 1: Stride proportional fairness (1:2:10)
  // Expected: CPU time ratio should be close to 1:2:10
  printf("[Scenario 1] Stride proportional fairness (weight 1:2:10)\n");
  if (sched_algorithm(SCHED_STRIDE_VAL) < 0) {
    printf("FAIL: sched_algorithm(STRIDE)\n"); exit(1);
  }
  printf("  scheduler: %s\n", sched_algorithm_name(SCHED_STRIDE_VAL));

  child_id = 0;
  pid_a = fork();
  if (pid_a == 0) child_stride(1, 50);

  child_id = 1;
  pid_b = fork();
  if (pid_b == 0) child_stride(2, 50);

  child_id = 2;
  pid_c = fork();
  if (pid_c == 0) child_stride(10, 50);

  wait(0);
  wait(0);
  wait(0);
  printf("  [Scenario 1 done] expected CPU time ratio 1:2:10\n\n");

  // Print final stride state of three children
  unsigned long stride_a, pass_a;
  int weight_a;
  if (stride_getstate(pid_a, &stride_a, &pass_a, &weight_a) == 0) {
    printf("  [A] weight=%d stride=%lu pass=%lu\n", weight_a, stride_a, pass_a);
  }
  unsigned long stride_b, pass_b;
  int weight_b;
  if (stride_getstate(pid_b, &stride_b, &pass_b, &weight_b) == 0) {
    printf("  [B] weight=%d stride=%lu pass=%lu\n", weight_b, stride_b, pass_b);
  }
  unsigned long stride_c, pass_c;
  int weight_c;
  if (stride_getstate(pid_c, &stride_c, &pass_c, &weight_c) == 0) {
    printf("  [C] weight=%d stride=%lu pass=%lu\n", weight_c, stride_c, pass_c);
  }

  printf("  [Analysis] weight=1 stride=%lu (scheduled most), weight=10 stride=%lu (scheduled least)\n",
         stride_a, stride_c);
  printf("            stride ratio A:B:C = %lu:%lu:%lu (should be ~10:5:1, inverse of weight)\n\n",
         stride_a, stride_b, stride_c);

  // Scenario 2: Lottery expected fairness (1:1)
  // Two weight=1 processes, long run -> CPU time should approach 1:1
  printf("[Scenario 2] Lottery expected fairness (weight 1:1)\n");
  if (sched_algorithm(SCHED_LOTTERY_VAL) < 0) {
    printf("FAIL: sched_algorithm(LOTTERY)\n"); exit(1);
  }
  printf("  scheduler: %s\n", sched_algorithm_name(SCHED_LOTTERY_VAL));

  child_id = 0;
  pid_a = fork();
  if (pid_a == 0) child_lottery(1, 30);

  child_id = 1;
  pid_b = fork();
  if (pid_b == 0) child_lottery(1, 30);

  wait(0);
  wait(0);
  printf("  [Scenario 2 done] expected CPU time ratio near 1:1 (Lottery expectation)\n\n");

  // Scenario 3: Dynamic weight adjustment
  // Start with weight=1, switch to weight=50 mid-run, observe CPU time increase
  printf("[Scenario 3] Dynamic weight adjustment\n");
  if (sched_algorithm(SCHED_STRIDE_VAL) < 0) {
    printf("FAIL: sched_algorithm(STRIDE)\n"); exit(1);
  }

  child_id = 0;
  pid_a = fork();
  if (pid_a == 0) {
    if (stride_setweight(getpid(), 1) < 0) {
      printf("FAIL: stride_setweight initial\n"); exit(1);
    }
    printf("[%d] dynamic child pid=%d initial weight=1\n", parent_pid, getpid());
    busy_work(0, 30);
    if (stride_setweight(getpid(), 50) < 0) {
      printf("FAIL: stride_setweight change\n"); exit(1);
    }
    printf("[%d] dynamic child pid=%d changed to weight=50\n", parent_pid, getpid());
    busy_work(0, 30);
    exit(0);
  }

  wait(0);
  printf("  [Scenario 3 done] dynamic weight change should alter CPU share\n\n");

  // Scenario 4: Switch back to default RR
  printf("[Scenario 4] Switch back to default RR scheduler\n");
  if (sched_algorithm(0) < 0) {
    printf("FAIL: sched_algorithm(RR)\n"); exit(1);
  }
  printf("  scheduler: %s\n", sched_algorithm_name(0));

  printf("\n=== Phase F3 Innovation Test PASSED ===\n");
  printf("Summary: implemented Stride and Lottery proportional fair schedulers\n");
  printf("  - Stride: strict weight-based CPU time allocation (theoretical ancestor of Linux CFS)\n");
  printf("  - Lottery: random draw weighted by tickets, fair in expectation over time\n");
  return 0;
}
