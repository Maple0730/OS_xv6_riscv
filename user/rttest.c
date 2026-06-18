// Phase F3: Real-time task deadline-miss statistics.
//
// We run several real-time tasks under EDF and report the
// fraction of jobs that met their deadline vs the fraction
// that missed it.
//
// Methodology:
//   1. Each job records its release time (the tick at which
//      rt_wait_period returned).
//   2. After doing its work, it records its completion time.
//   3. A job is "missed" if (completion - release) > period.
//   4. The test reports per-task miss counts and the overall
//      deadline meet ratio.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

#define SHM_KEY 4242 // unique key for rt-stats (do not collide with pc_monitor's 4242)

struct rt_stats {
  int n_met;
  int n_missed;
};

#define NPERIODS 5

static int shmid = -1;
static struct rt_stats *stats = 0;

static void
rt_task(const char *name, int period, int cost)
{
  int me = getpid();
  uint64 shm_addr;
  // Attach to the shared stats region.
  if (shmat(SHM_KEY, &shm_addr) < 0) {
    printf("[%s pid=%d] shmat FAILED\n", name, me);
    exit(1);
  }
  stats = (struct rt_stats *)shm_addr;
  if (rt_register(period, cost) < 0) {
    printf("[%s pid=%d] rt_register FAILED\n", name, me);
    exit(1);
  }
  printf("[%s pid=%d] period=%d cost=%d\n", name, me, period, cost);

  for (int i = 0; i < NPERIODS; i++) {
    // Wait for the next period.
    if (rt_wait_period() < 0) {
      printf("[%s pid=%d] rt_wait_period FAILED\n", name, me);
      exit(1);
    }
    unsigned long release = cgettimeofday();
    // Simulate "cost" ticks of CPU work.
    volatile unsigned long busy = (unsigned long)cost * 150000UL;
    for (volatile unsigned long x = 0; x < busy; x++) {
      x = x * 1103515245 + 12345;
      if (x == 0xdeadbeefUL) printf("(never)\n");
    }
    unsigned long done = cgettimeofday();
    unsigned long elapsed = done - release;
    int period_us = period * 1000;  // 1 tick ≈ 1 ms
    if (elapsed > (unsigned long)period_us) {
      stats->n_missed++;
      printf("[%s pid=%d] period %d MISSED: elapsed=%lu us > period=%d us\n",
             name, me, i, elapsed, period_us);
    } else {
      stats->n_met++;
      printf("[%s pid=%d] period %d met: elapsed=%lu us <= period=%d us\n",
             name, me, i, elapsed, period_us);
    }
  }
  shmdt(shm_addr);
  exit(0);
}

int
main(int argc, char *argv[])
{
  printf("=== Phase F3: Real-Time Deadline Meet Ratio ===\n");

  // Switch to EDF.
  if (sched_algorithm(5) < 0) {
    printf("FAIL: sched_algorithm(EDF)\n");
    exit(1);
  }
  deadlock_set(0);

  // Allocate shared memory for stats.
  shmid = shmget(SHM_KEY, 64, 0x200);
  if (shmid < 0) {
    printf("FAIL: shmget\n");
    exit(1);
  }
  uint64 shm_addr;
  if (shmat(SHM_KEY, &shm_addr) < 0) {
    printf("FAIL: shmat\n");
    exit(1);
  }
  stats = (struct rt_stats *)shm_addr;
  stats->n_met = 0;
  stats->n_missed = 0;
  // shmdt(shm_addr); — disabled

  // Three tasks whose total utilisation is under 1.0:
  //   H: period=10 cost=2  (u=0.2)
  //   M: period=20 cost=4  (u=0.2)
  //   L: period=40 cost=4  (u=0.1)
  // total u=0.5, so all jobs should meet their deadlines.
  int p1 = fork();
  if (p1 == 0) rt_task("H", 10, 2);
  int p2 = fork();
  if (p2 == 0) rt_task("M", 20, 4);
  int p3 = fork();
  if (p3 == 0) rt_task("L", 40, 4);

  for (int i = 0; i < 3; i++) wait(0);

  // Re-attach to read final stats.
  if (shmat(SHM_KEY, &shm_addr) < 0) {
    printf("FAIL: shmat (parent)\n");
    exit(1);
  }
  stats = (struct rt_stats *)shm_addr;

  int total = stats->n_met + stats->n_missed;
  int meet_pct = total > 0 ? (stats->n_met * 100 / total) : 0;
  printf("Total jobs: %d  met: %d  missed: %d  meet ratio: %d%%\n",
         total, stats->n_met, stats->n_missed, meet_pct);
  int n_missed = stats->n_missed;
  shmdt(shm_addr);
  if (n_missed == 0) {
    printf("=== Phase F3 PASSED ===\n");
    exit(0);
  } else {
    printf("=== Phase F3 FAILED: %d deadline misses ===\n", n_missed);
    exit(1);
  }
}
