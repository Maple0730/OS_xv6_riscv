// SJF test using semaphores as a barrier.
//
// All children register their burst and then block on a semaphore.
// Parent waits for all N children to register, then posts the semaphore
// to release them. By the time they start the busy loop, all bursts
// are visible to the scheduler and SJF picks the smallest.

#include "kernel/types.h"
#include "user/user.h"

#define N 4

int
main(int argc, char *argv[])
{
  int pid;
  int start_tick;

  printf("=== SJF Test (sem barrier) ===\n");

  // Create two semaphores: a barrier (children wait, parent posts N times)
  // and a ready counter (children post, parent waits N times).
  // Children inherit these IDs through fork (semtable is global).
  int sem_barrier_id = sem_open(0);  // initialized to 0, children block here
  int sem_ready_id   = sem_open(0);  // initialized to 0, children post here
  if (sem_barrier_id < 0 || sem_ready_id < 0) {
    printf("sem_open failed: b=%d r=%d\n", sem_barrier_id, sem_ready_id);
    exit(1);
  }

  // Keep using MLFQ (default) for child creation so all children get scheduled
  // and registered before we switch to SJF.

  int est[N] = {8, 4, 2, 1};
  int work_millions[N] = {8, 4, 2, 1};

  for (int i = 0; i < N; i++) {
    pid = fork();
    if (pid == 0) {
      // ---------- child ----------
      // Set burst FIRST, before signaling ready.
      sched_setburst(getpid(), est[i]);
      // Signal parent that burst is set
      sem_post(sem_ready_id);
      // Block until parent releases all children
      sem_wait(sem_barrier_id);

      int t0 = uptime();
      volatile long sum = 0;
      for (volatile long j = 0; j < (long)work_millions[i] * 1000000L; j++) {
        sum += j;
      }
      int t1 = uptime();
      printf("[child %d] est=%d work=%dM FINISHED at tick %d (took %d ticks)\n",
             i, est[i], work_millions[i], t1, t1 - t0);
      exit(0);
    }
  }

  // ---------- parent ----------
  // Wait for all N children to set their bursts
  for (int i = 0; i < N; i++)
    sem_wait(sem_ready_id);

  // All bursts are set. Now switch to SJF.
  printf("All children registered bursts, switching to SJF.\n");
  sched_algorithm(3);
  start_tick = uptime();

  // Release all children
  for (int i = 0; i < N; i++)
    sem_post(sem_barrier_id);

  for (int i = 0; i < N; i++)
    wait(0);
  int end = uptime();

  printf("Total: %d ticks\n", end - start_tick);
  printf("Expected SJF order: 3 (est=1) -> 2 (est=2) -> 1 (est=4) -> 0 (est=8)\n");

  sched_algorithm(2);
  return 0;
}
