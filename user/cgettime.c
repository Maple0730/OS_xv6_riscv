#include "kernel/types.h"
#include "user/user.h"

// QEMU RISC-V virt machine typically runs mtime at 10MHz (10,000,000 Hz)
// This is configurable in QEMU but the default is 10MHz
#ifndef CLOCK_HZ
#define CLOCK_HZ 10000000
#endif
#define TICKS_PER_SEC 100
#define US_PER_SEC 1000000

int main(int argc, char *argv[]) {
  uint64 t1, t2, diff;
  int ticks;
  int i;
  uint64 min_cycles = (uint64)-1;
  uint64 max_cycles = 0;
  uint64 total_cycles = 0;

  printf("=== High-Precision Timer Test (cgettimeofday) ===\n\n");

  // Part 1: Compare cgettimeofday() with uptime()
  printf("1. Comparing cgettimeofday() with uptime():\n");
  t1 = cgettimeofday();
  ticks = uptime();
  t2 = cgettimeofday();

  printf("   uptime() = %d ticks\n", ticks);
  printf("   uptime*%d = %lu (uptime in microseconds, TICKS_PER_SEC=%d)\n",
         US_PER_SEC, (uint64)ticks * US_PER_SEC / TICKS_PER_SEC, TICKS_PER_SEC);
  printf("   cgettimeofday() = %lu (raw cycle count, start)\n", t1);
  printf("   cgettimeofday() = %lu (raw cycle count, after uptime)\n", t2);
  printf("   delta between two cgettimeofday calls = %lu cycles (~%.2f us)\n\n",
         t2 - t1, (double)(t2 - t1) / (CLOCK_HZ / US_PER_SEC));

  // Part 2: Show microsecond conversion
  printf("2. Microsecond conversion (clock=%d Hz):\n", CLOCK_HZ);
  printf("   %lu cycles / (%d / %d) = %.2f microseconds\n",
         t1, CLOCK_HZ, US_PER_SEC, (double)t1 / (CLOCK_HZ / US_PER_SEC));
  printf("   %lu cycles / (%d / %d) = %.2f microseconds\n",
         t2, CLOCK_HZ, US_PER_SEC, (double)t2 / (CLOCK_HZ / US_PER_SEC));
  printf("   Note: CLOCK_HZ may vary - check your QEMU config\n\n");

  // Part 3: Busy loop timing test
  printf("3. Busy loop timing (10 iterations, ~100000 ops each):\n\n");

  for (i = 0; i < 10; i++) {
    t1 = cgettimeofday();

    volatile int x = 0;
    int j;
    for (j = 0; j < 100000; j++) {
      x++;
    }

    t2 = cgettimeofday();
    diff = t2 - t1;

    total_cycles += diff;
    if (diff < min_cycles) min_cycles = diff;
    if (diff > max_cycles) max_cycles = diff;

    printf("   Iter %d: %lu cycles (%.2f us)\n",
           i + 1, diff, (double)diff / (CLOCK_HZ / US_PER_SEC));
  }

  printf("\n   Statistics:\n");
  printf("   Min: %lu cycles (%.2f us)\n", min_cycles, (double)min_cycles / (CLOCK_HZ / US_PER_SEC));
  printf("   Max: %lu cycles (%.2f us)\n", max_cycles, (double)max_cycles / (CLOCK_HZ / US_PER_SEC));
  printf("   Avg: %lu cycles (%.2f us)\n",
         total_cycles / 10, (double)(total_cycles / 10) / (CLOCK_HZ / US_PER_SEC));

  printf("\n=== Test Complete ===\n");
  exit(0);
}
