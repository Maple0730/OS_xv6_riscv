// Dining Philosophers — Deadlock Reproduction (Phase B1)
//
// Test purpose:
//   Reproduce the classic "Dining Philosophers" deadlock.
//   Five philosophers sit around a table, each with a fork on the left
//   and a fork on the right.  Each philosopher:
//     1) picks up the left fork (sem_wait left)
//     2) picks up the right fork (sem_wait right)
//     3) eats (busy_wait ticks)
//     4) puts both forks back (sem_post)
//
//   When all five philosophers simultaneously grab their LEFT fork, the
//   system enters a circular wait — a deadlock.  Each philosopher holds
//   one fork and is waiting for the next one, which is held by the next
//   philosopher.  The expected observation is that all 5 child processes
//   end up in SLEEPING state and there is no progress.
//
// Deadlock conditions demonstrated (all 4):
//   1. Mutual exclusion: each fork (semaphore with value 1) can only be
//      held by one philosopher at a time.
//   2. Hold and wait: a philosopher that holds the left fork waits for
//      the right fork.
//   3. No preemption: a fork cannot be forcibly taken away; it can only
//      be released voluntarily by sem_post.
//   4. Circular wait: P0 -> P1 -> P2 -> P3 -> P4 -> P0.

#include "kernel/types.h"
#include "user/user.h"
#include "sem.h"

#define NPHIL 5
#define EAT_TICKS 30    // how long "eating" takes (ticks)
#define ROUNDS    100   // how many times each philosopher eats

static sem_t forks[NPHIL];

// Philosopher i: forks on the table are numbered 0..NPHIL-1.
//   left  = i
//   right = (i + 1) % NPHIL
//
// We intentionally grab LEFT first, then RIGHT — the classic deadlock
// pattern.  All NPHIL processes race for their left fork; once every
// left fork is taken, every philosopher blocks on the right one.
static void
philosopher(int id)
{
  int left  = id;
  int right = (id + 1) % NPHIL;
  int eaten = 0;

  for (int round = 0; round < ROUNDS; round++) {
    // 1) Acquire left fork.
    printf("  [phil %d] take LEFT  fork=%d\n", id, left);
    sem_wait(forks[left]);

    // 2) Try to acquire right fork.  For the first round we DELIBERATELY
    //    make every philosopher synchronize here so all NPHIL left-fork
    //    holders compete simultaneously for their right fork.
    //    A short pause lets the others reach the same point.
    for (int p = 0; p < 5; p++) pause(1);

    printf("  [phil %d] try  RIGHT fork=%d\n", id, right);
    sem_wait(forks[right]);

    // 3) Eat.
    printf("  [phil %d] EATING  (round %d)\n", id, round);
    for (int t = 0; t < EAT_TICKS; t++) pause(1);
    eaten++;

    // 4) Release right then left.
    sem_post(forks[right]);
    sem_post(forks[left]);
    printf("  [phil %d] done round %d\n", id, round);
  }

  printf("  [phil %d] FINISHED  total_eaten=%d\n", id, eaten);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int pid;

  printf("=== Dining Philosophers — Deadlock Reproduction ===\n");
  printf("Phase B1:  demonstrates the 4 necessary conditions of deadlock\n");
  printf("NPHIL = %d, EAT_TICKS = %d, ROUNDS = %d\n\n", NPHIL, EAT_TICKS, ROUNDS);

  // Create NPHIL forks, each initialized to 1 (binary semaphore).
  for (int i = 0; i < NPHIL; i++) {
    forks[i] = sem_open(1);
    if (forks[i] < 0) {
      printf("FAIL: sem_open failed for fork %d\n", i);
      exit(1);
    }
  }
  printf("Created %d fork semaphores (init=1)\n\n", NPHIL);

  // Spawn NPHIL child processes.
  printf("Forking %d philosopher processes...\n", NPHIL);
  for (int i = 0; i < NPHIL; i++) {
    pid = fork();
    if (pid < 0) {
      printf("FAIL: fork failed for phil %d\n", i);
      exit(1);
    }
    if (pid == 0) {
      // Child: become philosopher i.
      philosopher(i);
      // not reached
    }
    printf("  [parent] forked phil %d, pid=%d\n", i, pid);
  }

  printf("\nAll %d philosophers started.  Expecting deadlock on round 0\n", NPHIL);
  printf("because every philosopher picks up the left fork first, then\n");
  printf("blocks on the right fork (circular wait).\n\n");

  // Wait for all children.  In a perfect deadlock, this will hang
  // forever; the user can press Ctrl-P to see all 5 in SLEEPING.
  // For the test to terminate "cleanly" we still call wait() — the
  // caller can manually abort with Ctrl-A X once the deadlock is
  // observed.  The next phases (B2/B3/B4) show how to *avoid* /
  // *detect+recover* from this exact state.
  for (int i = 0; i < NPHIL; i++) {
    wait(0);
  }

  for (int i = 0; i < NPHIL; i++) sem_close(forks[i]);

  printf("\n=== Dining Test (unexpectedly) Completed ===\n");
  return 0;
}
