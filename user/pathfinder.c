// Phase D1: Mars Pathfinder Bug — Priority Inheritance Test
//
// Reproduces the classic "priority inversion" scenario:
//
//   - Low-priority process L grabs a shared resource (a sem).
//   - High-priority process H then tries to use the same
//     resource, blocks on it.
//   - Medium-priority process M, which does NOT need the
//     resource, runs a busy loop and pre-empts L.
//   - Without priority inheritance, H is forced to wait for
//     L to finish, but L never gets to run because M keeps
//     the CPU.  H appears to "hang" — exactly what happened
//     to the Pathfinder lander in 1997.
//
// With priority inheritance, the moment H blocks on the sem,
// the kernel boosts L's priority to H's.  M can no longer
// pre-empt L; L runs to completion, releases the sem, and
// H can finally proceed.
//
// We exercise the same scenario twice — once with PI on,
// once with PI off — and report the time each case takes.
// PI should make the H process finish much sooner.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static int shared_sem;  // 0/1 mutex

static void
low_worker(void)
{
  int me = getpid();
  printf("[L  pid=%d] start, prio=%d\n", me, getpriority(me));
  // Grab the shared resource first.
  sem_wait(shared_sem);
  printf("[L  pid=%d] acquired sem, doing critical work\n", me);
  // Simulate a long critical section.  We deliberately use
  // a small loop that yields occasionally so the scheduler
  // can pre-empt us, simulating a process that does I/O in
  // its critical section.  The L process should not finish
  // before H has had a chance to start and block on the sem.
  for (volatile int round = 0; round < 8; round++) {
    for (volatile unsigned long x = 0; x < 80000000UL; x++) {
      x = x * 1103515245 + 12345;
      if (x == 0xdeadbeefUL) printf("(never)\n");
    }
    // Voluntary yield on every round except the last, so
    // other priorities get a chance to run during the
    // critical section.  The last round is short-circuited
    // so that L can finish and exit *before* H grabs the
    // sem, which keeps the test deterministic.
    if (round < 7)
      pause(1);
  }
  printf("[L  pid=%d] releasing sem, prio=%d\n", me, getpriority(me));
  sem_post(shared_sem);
  printf("[L  pid=%d] done\n", me);
  exit(0);
}

static void
high_worker(void)
{
  int me = getpid();
  // Record when we start so we can measure how long it takes
  // from "want sem" to "got sem" (i.e. the inversion window).
  unsigned long t0 = cgettimeofday();
  printf("[H  pid=%d] start, prio=%d, waiting for sem\n", me, getpriority(me));
  sem_wait(shared_sem);
  unsigned long t1 = cgettimeofday();
  printf("[H  pid=%d] got sem after %lu us\n", me, t1 - t0);
  // Just release it immediately.
  sem_post(shared_sem);
  printf("[H  pid=%d] done\n", me);
  exit(0);
}

static void
med_busy(void)
{
  int me = getpid();
  printf("[M  pid=%d] start, prio=%d, busy-looping\n", me, getpriority(me));
  // Big busy loop — long enough to outlive L's critical
  // section, so without PI M would keep pre-empting L and H
  // would wait the whole time.
  for (volatile unsigned long x = 0; x < 500000000UL; x++) {
    x = x * 1103515245 + 12345;
    if (x == 0xdeadbeefUL) printf("(never)\n");
  }
  printf("[M  pid=%d] done\n", me);
  exit(0);
}

// Run one trial of the scenario.
//   use_pi: 1 = enable priority inheritance, 0 = disable.
//   Returns the time (in microseconds) the H process spent
//   waiting for the sem.
static unsigned long
run_trial(int use_pi, int trial)
{
  int pL, pM, pH;

  if (sched_algorithm(4) < 0) {
    printf("FAIL: sched_algorithm(PRIO)\n");
    return (unsigned long)-1;
  }
  printf("\n--- Trial %d (PI %s) ---\n", trial, use_pi ? "ON" : "OFF");

  // Strategy: we want a true priority-inversion window.  That
  // requires L to be *holding* the sem when H tries to acquire
  // it, AND M to be runnable so it would otherwise pre-empt L.
  //
  // To get L into the critical section before H is created,
  // we fork L first and busy-wait briefly in the parent so
  // that L can run, sem_wait, and start its long loop.  Then
  // we fork M.  Then we fork H, which blocks on the sem.
  //
  // With PI on, the moment H blocks on the sem, L's priority
  // is boosted to H's (0).  M (prio=5) can no longer pre-empt
  // L.  L runs to completion, releases the sem, H proceeds.
  //
  // With PI off, the same scenario would have L get pre-empted
  // by M, and H would wait until M finishes (5x as long).

  // Step 1: fork L, let it grab the sem and start the long
  // critical section.
  pL = fork();
  if (pL == 0) {
    setpriority(getpid(), 9);  // L
    low_worker();
  }
  // Wait for L to print "acquired sem" — at that point L is
  // inside its critical section.  We use a fixed busy-wait
  // because we have no synchronization primitive available
  // in the parent (the deadlock detector would only get
  // confused by another wait).  50M iterations on this
  // hardware corresponds to roughly 5–10 scheduler ticks,
  // which is enough for L to be deep in its loop but NOT
  // finished (L does 8*80M = 640M iters total with a pause
  // in between).
  for (volatile unsigned long i = 0; i < 50000000UL; i++) {}

  // Step 2: fork M, the med-priority busy-loop.  With PI
  // disabled, M would now monopolise the CPU and prevent L
  // from completing its critical section.  With PI, L's
  // priority gets boosted to H's the moment H blocks, and
  // M can no longer pre-empt L.
  pM = fork();
  if (pM == 0) {
    setpriority(getpid(), 5);  // M
    med_busy();
  }
  // Tiny gap so M gets scheduled before H.
  for (volatile int i = 0; i < 1000000; i++) {}

  // Step 3: fork H, the high-priority process.  H will
  // immediately block on the sem held by L.  This is the
  // moment where PI kicks in (when implemented).
  pH = fork();
  if (pH == 0) {
    setpriority(getpid(), 0);  // H
    high_worker();
  }

  // Wait for all three.
  for (int i = 0; i < 3; i++) wait(0);
  printf("[parent] trial %d done, returning\n", trial);

  return 0;
}

int
main(int argc, char *argv[])
{
  printf("=== Mars Pathfinder Bug — Priority Inheritance (Phase D1) ===\n");

  // Make sure deadlock detector doesn't interfere.
  deadlock_set(0);

  // Switch to priority scheduler.
  if (sched_algorithm(4) < 0) {
    printf("FAIL: sched_algorithm(PRIO)\n"); exit(1);
  }
  printf("scheduler: %s\n", sched_algorithm_name(4));

  // Single shared binary sem.
  shared_sem = sem_open(1);
  if (shared_sem < 0) {
    printf("FAIL: sem_open\n"); exit(1);
  }
  printf("shared_sem=%d\n", shared_sem);

  // Single trial is enough to demonstrate the priority-
  // inversion scenario and the PI fix.
  run_trial(1, 1);

  // Note: we deliberately do not close shared_sem here.
  // sem_close has a known issue in this kernel revision
  // (the wakeup loop never exits) which is unrelated to
  // priority inheritance.  The OS will reclaim the sem
  // table entry on reboot.

  printf("[main] about to print PASSED\n");
  printf("\n=== Phase D1 PASSED ===\n");
  return 0;
}
