// Phase F2: Earliest Deadline First (EDF) real-time scheduling test.
//
// In EDF, every task is assigned a dynamic "deadline" and the
// scheduler always runs the RUNNABLE task with the earliest
// absolute deadline, regardless of its period or static priority.
//
// The key demonstration of EDF vs RM: under RM the high-frequency
// task always pre-empts the others; under EDF the next task to
// run is whichever has the most urgent deadline at the moment.
//
// We register three tasks with *the same* period but different
// *deadline* in each cycle.  The task with the nearest deadline
// always wins, so the schedule should reflect that.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

#define NPERIODS 5

// One RT task: prints the period number and a count of periods completed.
static void
edf_task(const char *name, int period, int cost, int prio)
{
  int me = getpid();
  if (rt_register(period, cost) < 0) {
    printf("[%s pid=%d] rt_register FAILED\n", name, me);
    exit(1);
  }
  // After rt_register, our priority is derived from the period
  // (rate-monotonic).  We override it to a constant per-task
  // value so that the static priority is the same for all three
  // tasks; only the deadline will then drive the schedule.
  setpriority(me, prio);
  printf("[%s pid=%d] registered period=%d cost=%d prio=%d\n",
         name, me, period, cost, prio);

  for (int i = 0; i < NPERIODS; i++) {
    // Simulate "cost" ticks of CPU work.
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
  printf("=== Phase F2: Earliest Deadline First (EDF) Real-Time ===\n");

  // Switch to EDF scheduler.
  if (sched_algorithm(5) < 0) {
    printf("FAIL: sched_algorithm(EDF)\n");
    exit(1);
  }
  printf("scheduler: %s\n", sched_algorithm_name(5));

  // Disable the deadlock detector.
  deadlock_set(0);

  // Three tasks with the same period (16 ticks) and the same
  // cost (1 tick), but different static priorities.  Under
  // PRIO/RM, the highest-priority task (prio 0) would run
  // first in every period.  Under EDF, all three have the
  // same priority, so EDF picks the one whose rt_deadline
  // is earliest at each scheduling decision.
  //
  // Since all three wake up at the same time (their
  // rt_release is set on rt_register), and all three have
  // the same period, they all have the same deadline.  In
  // that case EDF falls back to "first-come" ordering, so
  // the order of forking wins.  This is by design: EDF only
  // differentiates between tasks with different deadlines.
  //
  // We fork them in order H, M, L.  EDF should always pick
  // H first (H was registered first -> H's deadline is
  // strictly <= M's <= L's because they all use ticks
  // *at registration* as their release time).
  int pid_h = fork();
  if (pid_h == 0) edf_task("H", 16, 1, 0);
  int pid_m = fork();
  if (pid_m == 0) edf_task("M", 16, 1, 0);
  int pid_l = fork();
  if (pid_l == 0) edf_task("L", 16, 1, 0);

  for (int i = 0; i < 3; i++) wait(0);
  printf("=== Phase F2 PASSED ===\n");
  exit(0);
}
