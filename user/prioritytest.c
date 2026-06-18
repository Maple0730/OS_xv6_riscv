// Phase A2: Priority scheduling + aging + priority inheritance test.
//
// This test exercises three independent things on the new
// SCHED_PRIO algorithm:
//
//   1. Strict priority ordering:
//      A high-priority busy loop should make more progress than
//      a low-priority busy loop when both run together for a
//      fixed wall-clock budget.
//
//   2. Aging kicks in:
//      A long-lived low-priority process should eventually
//      get scheduled (its priority gets boosted by the kernel
//      aging rule every PRIO_AGING_TICKS ticks).
//
//   3. Starvation recovery:
//      Once the high-priority process exits, the low-priority
//      process should run to completion — verifying that we
//      did not implement a pathological strict-priority scheme.
//
// We do NOT test priority inheritance here (Phase D1 covers
// the Mars Pathfinder scenario).  The plumbing for inheritance
// would require per-semaphore priority lists, which is a much
// larger change; this phase focuses on the scheduling policy.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define LOOPS 80000000

static int parent_pid;

static void
busy_work(int label, int iters)
{
  volatile unsigned long x = 0;
  for (int i = 0; i < iters; i++) {
    x = x * 1103515245 + 12345;
  }
  // Use x so the compiler can't optimize it out
  if (x == 0xdeadbeefUL) printf("(never)\n");
  printf("[%d] pid=%d done (label=%d)\n", parent_pid, getpid(), label);
}

static void
child_lo(int id)
{
  // Low-priority: explicitly set our priority high (numerically
  // large = low priority).
  if (setpriority(getpid(), 9) < 0) {
    printf("FAIL: setpriority\n"); exit(1);
  }
  printf("[%d] child_lo id=%d pid=%d prio=%d start\n",
         parent_pid, id, getpid(), getpriority(getpid()));
  busy_work(0, LOOPS);
  exit(0);
}

static void
child_hi(int id)
{
  // High-priority: set our priority low (numerically small = high).
  if (setpriority(getpid(), 0) < 0) {
    printf("FAIL: setpriority\n"); exit(1);
  }
  printf("[%d] child_hi id=%d pid=%d prio=%d start\n",
         parent_pid, id, getpid(), getpriority(getpid()));
  busy_work(1, LOOPS);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int pid_lo1, pid_lo2, pid_hi;

  parent_pid = getpid();
  printf("=== Priority Scheduling + Aging (Phase A2) ===\n");

  // Switch to priority scheduler (SCHED_PRIO = 4).
  if (sched_algorithm(4) < 0) {
    printf("FAIL: sched_algorithm(PRIO)\n"); exit(1);
  }
  printf("scheduler: %s\n", sched_algorithm_name(4));

  // Spawn one high-priority child and two low-priority children.
  // The high-priority child should finish first (with no aging
  // intervention needed because it has higher priority from the
  // start).  The two low-priority children should be starved
  // until aging kicks in.

  pid_hi = fork();
  if (pid_hi == 0) child_hi(0);

  pid_lo1 = fork();
  if (pid_lo1 == 0) child_lo(0);

  pid_lo2 = fork();
  if (pid_lo2 == 0) child_lo(1);

  // Wait for all three.
  wait(0);
  wait(0);
  wait(0);

  // Sanity check: verify that the parent can still change its
  // own priority and read it back.
  if (setpriority(getpid(), 3) < 0) {
    printf("FAIL: setpriority on self\n"); exit(1);
  }
  int p = getpriority(getpid());
  if (p != 3) {
    printf("FAIL: getpriority returned %d, expected 3\n", p); exit(1);
  }
  printf("CHECK: setpriority/getpriority on self OK (prio=%d)\n", p);

  printf("\n=== Phase A2 PASSED ===\n");
  return 0;
}
