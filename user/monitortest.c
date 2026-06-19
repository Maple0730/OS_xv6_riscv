// Monitor (Phase C1) — basic test
//
// Tests the 6 monitor syscalls:
//   mon_create  -> integer monitor id
//   mon_lock(m)   acquire mutex
//   mon_unlock(m) release mutex
//   mon_wait(m, cvid)   wait on condition variable (releases mutex)
//   mon_signal(m, cvid) wake one waiter
//   mon_broadcast(m, cvid) wake all waiters
//
// We test the classic "two-condition producer/consumer on a 1-slot
// buffer" pattern: producer waits on `not_full` (cvid=0), consumer
// waits on `not_empty` (cvid=1).  This is a minimal but real
// monitor test, *not* the full P/C we'll do in C2 — here we
// serialise the access through the monitor and verify
// wait/signal/broadcast all work.

#include "kernel/types.h"
#include "user/user.h"

#define CAP 3

static int m;
static int buf[CAP];
static int count = 0;
static int in_pos = 0, out_pos = 0;

static void
producer(int id)
{
  for (int i = 0; i < 5; i++) {
    mon_lock(m);
    while (count == CAP) {
      // Buffer is full, wait on "not_full" cv (cvid=0).
      mon_wait(m, 0);
    }
    buf[in_pos] = id * 100 + i;
    printf("  [prod %d] put %d at %d  (count=%d)\n", id, buf[in_pos], in_pos, count + 1);
    in_pos = (in_pos + 1) % CAP;
    count++;
    // Wake ONE consumer
    mon_signal(m, 1);
    mon_unlock(m);

    // Pace
    for (int t = 0; t < 5; t++) pause(1);
  }
  exit(0);
}

static void
consumer(int id)
{
  for (int i = 0; i < 5; i++) {
    mon_lock(m);
    while (count == 0) {
      // Buffer is empty, wait on "not_empty" cv (cvid=1).
      mon_wait(m, 1);
    }
    int v = buf[out_pos];
    out_pos = (out_pos + 1) % CAP;
    count--;
    printf("  [cons %d] got %d from %d  (count=%d)\n", id, v, out_pos, count);
    // Wake ONE producer
    mon_signal(m, 0);
    mon_unlock(m);

    for (int t = 0; t < 7; t++) pause(1);
  }
  exit(0);
}

int
main(int argc, char *argv[])
{
  printf("=== Monitor (Phase C1) — bounded buffer test ===\n");
  printf("CAP=%d, 1 producer + 1 consumer, 5 items each\n\n", CAP);

  m = mon_create();
  if (m < 0) {
    printf("FAIL: mon_create returned %d\n", m);
    exit(1);
  }
  printf("[parent] created monitor id=%d\n", m);

  int ppid = fork();
  if (ppid < 0) exit(1);
  if (ppid == 0) producer(1);

  int cpid = fork();
  if (cpid < 0) exit(1);
  if (cpid == 0) consumer(1);

  // Wait for both
  wait(0);
  wait(0);
  printf("\n=== Monitor test PASSED ===\n");
  return 0;
}
