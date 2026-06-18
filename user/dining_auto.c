// Dining Philosophers — auto-recovery (Phase B4)
//
// Same as user/dining.c (B1) — the kernel's deadlock detector
// (`kernel/deadlock_detect.c`) periodically scans the wait-for
// graph and aborts the youngest process in a detected cycle.
//
// Output should include:
//   [DEADLOCK] cycle detected, length=N: pid=X -> pid=Y -> ...
//   [DEADLOCK] aborting victim pid=...

#include "kernel/types.h"
#include "user/user.h"
#include "sem.h"

#define NPHIL 5
#define EAT_TICKS 30
#define ROUNDS    5   // each phil eats 5 rounds; if no deadlock, exits

static sem_t forks[NPHIL];

static void
philosopher(int id)
{
  int left  = id;
  int right = (id + 1) % NPHIL;
  int eaten = 0;

  for (int round = 0; round < ROUNDS; round++) {
    sem_wait(forks[left]);
    for (int p = 0; p < 5; p++) pause(1);  // sync to LEFT-acquire point
    sem_wait(forks[right]);
    for (int t = 0; t < EAT_TICKS; t++) pause(1);
    eaten++;
    sem_post(forks[right]);
    sem_post(forks[left]);
  }
  exit(0);
}

int
main(int argc, char *argv[])
{
  int pid;

  printf("=== Dining — auto-recovery test (Phase B4) ===\n");
  printf("Kernel will detect deadlock and abort youngest phil\n");
  printf("Survivors (4 of 5) finish their rounds and exit.\n\n");

  for (int i = 0; i < NPHIL; i++) {
    forks[i] = sem_open(1);
    if (forks[i] < 0) exit(1);
  }

  for (int i = 0; i < NPHIL; i++) {
    pid = fork();
    if (pid < 0) exit(1);
    if (pid == 0) {
      philosopher(i);
    }
  }

  // Wait for all children.  If the deadlock detector works, 4 of
  // the 5 will finish normally and the 5th will be killed by the
  // kernel.  Either way, all 5 pids eventually get reaped.
  for (int i = 0; i < NPHIL; i++) {
    if (wait(0) < 0) {
      printf("[parent] wait failed (some child was already aborted?)\n");
    }
  }

  for (int i = 0; i < NPHIL; i++) sem_close(forks[i]);

  printf("\n=== Dining auto-recovery test DONE ===\n");
  return 0;
}
