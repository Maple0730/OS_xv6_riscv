// Dining Philosophers — Deadlock Prevention: destroy "Hold & Wait"
//
// Test purpose:
//   Prevent the deadlock demonstrated in B1 by BREAKING the
//   "hold and wait" condition.
//
// Technique: add a "room" semaphore (init = NPHIL - 1) that
//   allows AT MOST NPHIL-1 philosophers to be seated at the
//   table simultaneously.  The (NPHIL-1)-th philosopher cannot
//   sit down until one of the seated philosophers leaves (i.e.,
//   puts down both forks).  Mathematical guarantee: with
//   NPHIL forks and at most NPHIL-1 contenders, at least one
//   philosopher can grab both forks — no circular wait can
//   form.
//
// Deadlock condition broken:
//   - Mutual exclusion  : still present (forks are exclusive).
//   - Hold and wait     : BROKEN — a philosopher may not be at
//                         the table at all (waiting on `room`),
//                         so it cannot be holding any fork while
//                         waiting.
//   - No preemption     : still present.
//   - Circular wait     : cannot form because at least one
//                         philosopher can always proceed.
//
// Expected: every philosopher completes all ROUNDS; no deadlock.

#include "kernel/types.h"
#include "user/user.h"
#include "sem.h"

#define NPHIL 5
#define EAT_TICKS 10
#define ROUNDS    10

static sem_t forks[NPHIL];
static sem_t room;        // counts free "seats"; init = NPHIL-1

static void
philosopher(int id)
{
  int left  = id;
  int right = (id + 1) % NPHIL;
  int eaten = 0;

  for (int round = 0; round < ROUNDS; round++) {
    // Wait for an available seat.
    sem_wait(room);

    // Acquire both forks in any order.
    sem_wait(forks[left]);
    sem_wait(forks[right]);

    // Eat.
    printf("  [phil %d] EATING round=%d (eaten=%d)\n", id, round, eaten);
    for (int t = 0; t < EAT_TICKS; t++) pause(1);
    eaten++;

    // Release forks then leave the seat.
    sem_post(forks[right]);
    sem_post(forks[left]);
    sem_post(room);
  }

  printf("  [phil %d] FINISHED  total_eaten=%d\n", id, eaten);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int pid;

  printf("=== Dining — Prevention A: break 'Hold and Wait' ===\n");
  printf("Phase B2-1:  NPHIL=%d, room semaphore (init=%d) caps concurrency\n\n", NPHIL, NPHIL - 1);

  for (int i = 0; i < NPHIL; i++) {
    forks[i] = sem_open(1);
    if (forks[i] < 0) {
      printf("FAIL: sem_open failed for fork %d\n", i);
      exit(1);
    }
  }
  room = sem_open(NPHIL - 1);
  if (room < 0) {
    printf("FAIL: sem_open(room) failed\n");
    exit(1);
  }
  printf("Created %d fork semaphores + 1 room semaphore\n\n", NPHIL);

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
  sem_close(room);

  printf("\n=== Dining Safe-1 PASSED (no deadlock) ===\n");
  return 0;
}
