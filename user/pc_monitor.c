// Producer-Consumer using a Monitor (Phase C2)
//
// Compare to user/semtest3.c which uses 3 semaphores (empty/full/
// mutex).  Here we use a single monitor with 2 condition variables:
//
//   cvid=0 -> "not_full"  (signalled by consumer)
//   cvid=1 -> "not_empty" (signalled by producer)
//
// Configuration mirrors semtest3: 2 producers, 2 consumers,
// 10 items each, buffer size 5.  We verify that produced count
// equals consumed count at the end.
//
// IMPORTANT: xv6 fork() gives each child a private copy of the
// parent's address space, so plain static variables such as
// `count` or `buf[]` are NOT shared between producer and
// consumer children.  We therefore use xv6's shared-memory
// facility (shmget/shmat) to put the buffer and the count in a
// region that all children can see.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define BUFFER_SIZE 5
#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2
#define ITEMS_PER_PRODUCER 10
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)

#define SHM_KEY 4242

// Single shared region layout:
//   ctrl[0] = count
//   ctrl[1] = in_pos
//   ctrl[2] = out_pos
//   ctrl[3] = produced_total
//   ctrl[4] = consumed_total
//   buf[0..BUFFER_SIZE-1]
static int m;
static int *ctrl;    // shared: see layout above
static int *buf;     // shared: BUFFER_SIZE ints after ctrl

static void
producer(int id)
{
  for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
    mon_lock(m);
    while (ctrl[0] == BUFFER_SIZE) {
      mon_wait(m, 0);  // wait on "not_full"
    }
    buf[ctrl[1]] = id * 1000 + i;
    ctrl[1] = (ctrl[1] + 1) % BUFFER_SIZE;
    ctrl[0]++;
    ctrl[3]++;  // produced_total
    mon_signal(m, 1);  // signal "not_empty"
    mon_unlock(m);
    for (int t = 0; t < 3; t++) pause(1);
  }
  exit(0);
}

static void
consumer(int id)
{
  for (int i = 0; i < TOTAL_ITEMS / NUM_CONSUMERS; i++) {
    mon_lock(m);
    while (ctrl[0] == 0) {
      mon_wait(m, 1);  // wait on "not_empty"
    }
    int dummy_v = buf[ctrl[2]];
    if (dummy_v < 0) printf("(never)\n");
    ctrl[2] = (ctrl[2] + 1) % BUFFER_SIZE;
    ctrl[0]--;
    ctrl[4]++;  // consumed_total
    mon_signal(m, 0);  // signal "not_full"
    mon_unlock(m);
    for (int t = 0; t < 3; t++) pause(1);
  }
  exit(0);
}

int
main(int argc, char *argv[])
{
  int pid;

  printf("=== Producer-Consumer via Monitor (Phase C2) ===\n");
  printf("Configuration:\n");
  printf("  Buffer size:    %d\n", BUFFER_SIZE);
  printf("  Producers:      %d (each %d items)\n", NUM_PRODUCERS, ITEMS_PER_PRODUCER);
  printf("  Consumers:      %d\n", NUM_CONSUMERS);
  printf("  Total expected: %d\n\n", TOTAL_ITEMS);

  // Single shm region: ctrl[5] + buf[BUFFER_SIZE] all in 1 page.
  int total_size = (5 + BUFFER_SIZE) * sizeof(int);
  int shmid = shmget(SHM_KEY, total_size, 0x200);
  if (shmid < 0) { printf("FAIL: shmget\n"); exit(1); }

  uint64 shm_addr;
  if (shmat(SHM_KEY, &shm_addr) < 0) { printf("FAIL: shmat\n"); exit(1); }
  ctrl = (int *)shm_addr;
  buf  = (int *)(shm_addr + 5 * sizeof(int));

  // Initialise control block.  Only the parent does this —
  // children inherit the mapping after fork() (shm pages are
  // shared in the kernel).
  ctrl[0] = 0;  // count
  ctrl[1] = 0;  // in_pos
  ctrl[2] = 0;  // out_pos
  ctrl[3] = 0;  // produced_total (shared)
  ctrl[4] = 0;  // consumed_total (shared)

  m = mon_create();
  if (m < 0) { printf("FAIL: mon_create\n"); exit(1); }
  printf("[parent] monitor id=%d, shm ctrl=%p buf=%p\n", m, ctrl, buf);

  // Disable deadlock detector for this test — monitor-style
  // producer/consumer traffic can transiently look like a
  // deadlock (4 procs sleeping on different sems) but the
  // program is making progress via signal/wakeup.
  int prev = deadlock_set(0);
  printf("[parent] deadlock detector: enabled=%d (set to 0 for this test)\n", prev);

  printf("Creating %d producers...\n", NUM_PRODUCERS);
  for (int i = 0; i < NUM_PRODUCERS; i++) {
    pid = fork();
    if (pid < 0) exit(1);
    if (pid == 0) producer(i);
  }

  printf("Creating %d consumers...\n", NUM_CONSUMERS);
  for (int i = 0; i < NUM_CONSUMERS; i++) {
    pid = fork();
    if (pid < 0) exit(1);
    if (pid == 0) consumer(i);
  }

  printf("Waiting for all workers...\n");
  for (int i = 0; i < NUM_PRODUCERS + NUM_CONSUMERS; i++) wait(0);

  printf("\n=== PC via Monitor test COMPLETED ===\n");
  printf("produced_total = %d\n", ctrl[3]);
  printf("consumed_total = %d\n", ctrl[4]);
  printf("final count    = %d (expect 0)\n", ctrl[0]);

  if (ctrl[3] == TOTAL_ITEMS
      && ctrl[4] == TOTAL_ITEMS
      && ctrl[0] == 0) {
    printf("CHECK: produced == consumed == %d  OK\n", TOTAL_ITEMS);
    printf("\n=== Phase C2 PASSED ===\n");
  } else {
    printf("CHECK: MISMATCH (expected produced=%d consumed=%d count=0)\n",
           TOTAL_ITEMS, TOTAL_ITEMS);
    printf("\n=== Phase C2 FAILED ===\n");
    exit(1);
  }

  // Detach shared memory before exit so proc_freepagetable
  // does not try to free shm pages (it would kfree a leaf PTE
  // and trigger "freewalk: leaf").
  shmdt(shm_addr);
  return 0;
}
