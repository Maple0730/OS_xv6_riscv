// Dining Philosophers — Deadlock Prevention: destroy "Circular Wait"
//
// Test purpose:
//   Prevent the deadlock demonstrated in B1 by BREAKING the
//   "circular wait" condition.
//
// Technique: number all resources (forks) 0..NPHIL-1, and force
//   every philosopher to acquire the LOWER-numbered fork first,
//   then the HIGHER-numbered fork.  An even-id philosopher's
//   "left" is lower than "right" naturally, but an odd-id one
//   has "left" = id > "right" = (id+1)%N — without re-ordering
//   this would be a problem.  We use a single rule: always pick
//   up the LOWER-numbered fork first.  This eliminates the cycle
//   P0 -> P1 -> ... -> P_{N-1} -> P0 because there is no longer
//   a circular dependency of waiting.
//
// Deadlock condition broken:
//   - Mutual exclusion  : still present.
//   - Hold and wait     : still present (each philosopher
//                         acquires 2 forks atomically is not
//                         strictly required here — the cycle is
//                         already broken).
//   - No preemption     : still present.
//   - Circular wait     : BROKEN — total order on resources.
//
// Expected: every philosopher completes all ROUNDS; no deadlock.

#include "kernel/types.h"
#include "user/user.h"
#include "sem.h"

#define NPHIL 5
#define EAT_TICKS 10
#define ROUNDS    10

static sem_t forks[NPHIL];

// Return the LOWER of two fork ids.
static inline int
imin(int a, int b) { return a < b ? a : b; }
static inline int
imax(int a, int b) { return a > b ? a : b; }

static void
philosopher(int id)
{
  int left  = id;
  int right = (id + 1) % NPHIL;
  int first  = imin(left, right);   // always lower-numbered first
  int second = imax(left, right);
  int eaten = 0;

  for (int round = 0; round < ROUNDS; round++) {
    // 1) Acquire the lower-numbered fork.
    sem_wait(forks[first]);

    // 2) Acquire the higher-numbered fork.
    sem_wait(forks[second]);

    // Eat.
    printf("  [phil %d] EATING round=%d (eaten=%d)\n", id, round, eaten);
    for (int t = 0; t < EAT_TICKS; t++) pause(1);
    eaten++;

    // Release in reverse order.
    sem_post(forks[second]);
    sem_post(forks[first]);
  }

  printf("  [phil %d] FINISHED  total_eaten=%d\n", id, eaten);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int pid;

  printf("=== Dining — Prevention B: break 'Circular Wait' ===\n");
  printf("Phase B2-2:  NPHIL=%d, every phil picks LOW-numbered fork first\n\n", NPHIL);

  for (int i = 0; i < NPHIL; i++) {
    forks[i] = sem_open(1);
    if (forks[i] < 0) {
      printf("FAIL: sem_open failed for fork %d\n", i);
      exit(1);
    }
  }
  printf("Created %d fork semaphores\n\n", NPHIL);

  for (int i = 0; i < NPHIL; i++) {
    pid = fork();
    if (pid < 0) {
      printf("FAIL: fork failed for phil %d\n", i);
      exit(1);
    }
    if (pid == 0) {
      philosopher(i);
    }
  }

  for (int i = 0; i < NPHIL; i++) wait(0);

  for (int i = 0; i < NPHIL; i++) sem_close(forks[i]);

  printf("\n=== Dining Safe-2 PASSED (no deadlock) ===\n");
  return 0;
}
