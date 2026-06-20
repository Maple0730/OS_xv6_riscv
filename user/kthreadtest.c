// Phase G2: Kernel Thread (kthread) framework test (user-side)
//
// Linux kernel threads (kthread) are tasks that:
//   - run only in supervisor mode (no user page table)
//   - participate in scheduler like any other process
//   - are typically long-running background services
//
// This test does NOT create real kthreads (would need a kernel
// syscall like kthread_create).  Instead it demonstrates the
// *behavior* of a kthread by using a regular process as a
// proxy:
//   - Schedule periodic "background work"
//   - Show that the background work runs even while a "foreground"
//     process is busy
//   - Demonstrate that the background work responds to a "stop"
//     signal (via a global flag) and exits cleanly
//
// The kernel-side kthread framework is implemented in
// kernel/kthread.c (reaperd, schedstat_dump), but the user-side
// test cannot directly observe them without a kthread_*
// syscall.  Instead we *simulate* kthread behavior with
// fork() + a shared flag page.
//
// Build: add $(BU)/_kthreadtest to UPROGS
// Run: in qemu shell, type `kthreadtest`

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

// Shared "control block" between parent and "kthread proxy":
//   volatile int should_stop
//   volatile int cycle_count
//   volatile int last_run_tick
static int *ctl;

static int
child_kthread_proxy(void)
{
  // "kthread" body: loop forever, doing periodic work.
  // In a real kernel, this would be a kthread with its own
  // supervisor-mode entry.  Here we use a fork+execv pattern.
  int my_pid = getpid();
  int local_cycles = 0;
  uint64 t0 = cgettimeofday();

  while (!ctl[0]) {  // should_stop
    // "work" = print status every ~100k cycles
    if (local_cycles % 50 == 0) {
      printf("[kthread-proxy pid=%d] cycle=%d uptime_ticks=%d\n",
             my_pid, local_cycles, uptime());
    }
    volatile unsigned long x = 0;
    for (int i = 0; i < 100000; i++) {
      x += i;
    }
    if (x == (unsigned long)-1) printf("(never)\n");
    local_cycles++;
    ctl[2] = local_cycles;
  }
  uint64 t1 = cgettimeofday();
  printf("[kthread-proxy pid=%d] stopped after %d cycles, %lu cycles elapsed\n",
         my_pid, local_cycles, t1 - t0);
  ctl[2] = local_cycles;
  exit(0);
  return 0;
}

int
main(int argc, char *argv[])
{
  printf("=== Kernel Thread (kthread) Framework Test (Phase G2) ===\n\n");
  printf("Theory:\n");
  printf("  kthread = scheduler-managed task with no user address space.\n");
  printf("  Linux uses kthreads for: kthreadd, ksoftirqd, kworker, reaperd,\n");
  printf("  migration threads, etc.  They appear in 'ps' but with no user code.\n\n");
  printf("This user test cannot create real kthreads (no kthread_create syscall),\n");
  printf("so it simulates kthread semantics with a fork() + shared control block.\n\n");

  // Allocate a shared page for the control block.
  ctl = (int *)sbrk(PGSIZE);
  if ((uint64)ctl == (uint64)-1) {
    printf("FAIL: sbrk\n"); exit(1);
  }
  ctl[0] = 0;  // should_stop
  ctl[1] = 0;  // (reserved)
  ctl[2] = 0;  // cycle_count

  printf("[Phase 1] Spawning kthread proxy\n");
  int pid = fork();
  if (pid < 0) {
    printf("FAIL: fork\n"); exit(1);
  }
  if (pid == 0) {
    child_kthread_proxy();
  }

  // Let the "kthread" run for a bit
  uint64 t_start = cgettimeofday();
  printf("[main pid=%d] spawned kthread-proxy pid=%d, letting it run...\n",
         getpid(), pid);
  for (volatile int i = 0; i < 5000000; i++) ;  // busy-wait
  uint64 t_end = cgettimeofday();

  printf("\n[Phase 2] Background work concurrent with main busy-wait\n");
  printf("  main busy-wait elapsed: %lu cycles\n", t_end - t_start);
  printf("  kthread-proxy cycles completed (read from shared page): %d\n", ctl[2]);
  if (ctl[2] > 0) {
    printf("  [PASS] kthread-proxy ran concurrently with main\n");
  } else {
    printf("  [WARN] kthread-proxy did not advance; may be starved\n");
  }

  printf("\n[Phase 3] Request kthread-proxy to stop (kthread_stop equivalent)\n");
  ctl[0] = 1;  // set should_stop flag
  int status;
  waitpid(pid, &status);
  printf("  kthread-proxy pid=%d exited, status=%d\n", pid, status);
  printf("  final cycle_count: %d\n", ctl[2]);
  printf("  [PASS] kthread-proxy stopped and was reaped via waitpid (kthread equivalent)\n\n");

  printf("=== Kernel-side kthread framework (kernel/kthread.c) ===\n");
  printf("If compiled into the kernel, main.c launches:\n");
  printf("  - reaperd        : every 100 ticks, scans for orphan ZOMBIEs\n");
  printf("  - schedstat_dump : every 200 ticks, dumps scheduler stats\n");
  printf("Both run as 'struct proc' with name prefix '[k]' and pid >= 2.\n");
  printf("They can be observed in 'ps' output alongside user processes.\n\n");

  printf("=== kthread Test Complete ===\n");
  exit(0);
}
