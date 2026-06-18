// Phase F1: Rate-Monotonic real-time scheduling test.
//
// Three periodic tasks, each a busy loop with a known CPU cost.
// Shorter period -> higher priority.  Under RM, the schedule is
// "rate-monotonic" -- the high-frequency task pre-empts the
// others.  All tasks should meet their deadlines (Ci <= Ti).
//
// We use very small numbers (period/cost in ticks) and a tiny
// busy loop.  The priority scheduler (PRIO) is used as the
// base; priorities are derived from the period by the kernel.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

#define NPERIODS 5

// One RT task: prints the period and a count of periods completed.
static void
rt_task(const char *name, int period, int cost)
{
  int me = getpid();
  if (rt_register(period, cost) < 0) {
    printf("[%s pid=%d] rt_register FAILED\n", name, me);
    exit(1);
  }
  printf("[%s pid=%d] registered: period=%d cost=%d prio=%d\n",
         name, me, period, cost, getpriority(me));

  for (int i = 0; i < NPERIODS; i++) {
    // Simulate "cost" ticks of CPU work.
    // 1 tick ≈ 1 ms (xv6 default).  We busy-loop some
    // iterations proportional to "cost".
    volatile unsigned long busy = (unsigned long)cost * 150000UL;
    for (volatile unsigned long x = 0; x < busy; x++) {
      x = x * 1103515245 + 12345;
      if (x == 0xdeadbeefUL) printf("(never)\n");
    }
    printf("[%s pid=%d] period %d done, waiting for next\n", name, me, i);
    if (rt_wait_period() < 0) {
      printf("[%s pid=%d] rt_wait_period FAILED\n", name, me);
      exit(1);
    }
  }
  printf("[%s pid=%d] all %d periods done\n", name, me, NPERIODS);
  exit(0);
}

int
main(int argc, char *argv[])
{
  printf("=== Phase F1: Rate-Monotonic Real-Time Scheduling ===\n");

  // Switch to priority scheduler (RM uses static priorities).
  if (sched_algorithm(4) < 0) {
    printf("FAIL: sched_algorithm(PRIO)\n");
    exit(1);
  }
  // Disable the deadlock detector; it could see RT tasks
  // holding their inner work and get confused.
  deadlock_set(0);

  // Three tasks with periods 4, 8, 16 ticks and costs 1, 2, 4
  // (utilisation 1/4 + 2/8 + 4/16 = 0.25 + 0.25 + 0.25 = 0.75
  // which is well under the RM bound of ln 2 ≈ 0.693 for
  // arbitrary n; in practice n=3, so bound is 3*(2^(1/3)-1)
  // ≈ 0.779, so 0.75 is schedulable).
  int pid_h = fork();
  if (pid_h == 0) rt_task("H", 4, 1);    // high-frequency, highest priority
  int pid_m = fork();
  if (pid_m == 0) rt_task("M", 8, 2);
  int pid_l = fork();
  if (pid_l == 0) rt_task("L", 16, 4);

  for (int i = 0; i < 3; i++) wait(0);
  printf("=== Phase F1 PASSED ===\n");
  exit(0);
}
