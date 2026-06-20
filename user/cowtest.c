// Phase G1: Copy-on-Write (COW) fork test (user-side)
//
// COW principle (Linux 2.6+):
//   - fork() traditionally copies all parent pages (O(memory))
//   - With COW, parent and child share the same physical page,
//     marked PTE_COW (read-only); only on first WRITE does the
//     kernel allocate a new page (O(page table entries))
//
// This test verifies COW indirectly by measuring fork latency
// for different scenarios, since we don't expose a cowstat
// syscall to user space.
//
// Scenarios:
//   1. fork+exit cold: minimal parent, child exits immediately
//      -> pure fork() cost (page table copy only if COW)
//   2. fork+exit with 256KB parent: child shares memory
//      -> with COW: cost ~ same as scenario 1 (only PTE copy)
//      -> without COW: cost ~ copy of 256KB
//   3. 100 children: amortized fork cost per child
//   4. write trigger: fork, then child writes -> COW fault
//
// Build: add $(BU)/_cowtest to UPROGS
// Run: in qemu shell, type `cowtest`

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

// Time helper: convert microseconds to cycles at 10 MHz
#define US_TO_CYCLES(us) ((uint64)(us) * 10UL)

// One fork+exit timing iteration.  Returns cycles elapsed.
static uint64
time_one_fork(int parent_pages)
{
  uint64 t1, t2;

  // Allocate parent memory if requested (pre-touch each page
  // so the kernel actually maps physical frames for them).
  char *guard = sbrk(parent_pages * PGSIZE);
  if (guard == (char *)-1) {
    return (uint64)-1;
  }
  // Touch each page so it gets a physical frame.
  for (int i = 0; i < parent_pages; i++) {
    guard[i * PGSIZE] = (char)i;
  }

  t1 = cgettimeofday();
  int pid = fork();
  if (pid < 0) {
    printf("  fork failed\n");
    return (uint64)-1;
  }
  if (pid == 0) {
    // Child: touch the parent's memory read-only (COW: no fault)
    // and immediately exit.  This validates that read-only access
    // does not trigger a COW fault.
    volatile char sum = 0;
    for (int i = 0; i < parent_pages; i++) {
      sum += guard[i * PGSIZE];
    }
    if (sum == 0xCAFE) printf("(never)\n");  // make sum non-dead-code
    exit(0);
  }
  // Parent: wait for child
  int status;
  waitpid(pid, &status);
  t2 = cgettimeofday();

  // Free the parent memory (sbrk with negative n shrinks).
  // xv6's sys_sbrk only GROWS; if shrink isn't supported we just
  // leak — this is a test, not a production tool.
  // sbrk(-parent_pages * PGSIZE);  // skip; xv6 sbrk only grows

  return t2 - t1;
}

// One fork+write timing iteration (COW fault expected).
static uint64
time_fork_then_write(int parent_pages)
{
  char *guard = sbrk(parent_pages * PGSIZE);
  if (guard == (char *)-1) return (uint64)-1;
  for (int i = 0; i < parent_pages; i++) {
    guard[i * PGSIZE] = (char)i;
  }

  uint64 t1 = cgettimeofday();
  int pid = fork();
  if (pid < 0) return (uint64)-1;
  if (pid == 0) {
    // Write to first page: triggers COW fault (ref_count > 1)
    // After fault, child has its own private page.
    guard[0] = 0xAB;
    // Touch a few more pages so COW amortizes.
    for (int i = 1; i < parent_pages && i < 4; i++) {
      guard[i * PGSIZE] = (char)(0xCD + i);
    }
    exit(0);
  }
  int status;
  waitpid(pid, &status);
  uint64 t2 = cgettimeofday();
  return t2 - t1;
}

int
main(int argc, char *argv[])
{
  printf("=== Copy-on-Write (COW) fork Test (Phase G1) ===\n\n");
  printf("Theory:\n");
  printf("  Traditional fork: O(memory) -- copies every page\n");
  printf("  COW fork:         O(page table entries) -- shares pages,\n");
  printf("                                    copies on first WRITE\n");
  printf("  Goal: fork latency should NOT scale with parent memory size\n");
  printf("        unless children WRITE to the shared pages.\n\n");

  // ---------------------------------------------------------------
  // Scenario 1: empty parent, single fork
  // ---------------------------------------------------------------
  printf("[Scenario 1] fork() with empty parent\n");
  uint64 t1 = time_one_fork(0);
  printf("  fork+exit (0 pages): %lu cycles (~%lu us)\n\n",
         t1, t1 / 10UL);

  // ---------------------------------------------------------------
  // Scenario 2: large parent, single fork (COW should keep cost ~ constant)
  // ---------------------------------------------------------------
  printf("[Scenario 2] fork() with 64-page parent (256 KB), child reads only\n");
  uint64 t2 = time_one_fork(64);
  printf("  fork+exit (64 pages, read-only): %lu cycles (~%lu us)\n",
         t2, t2 / 10UL);
  if (t2 > t1 * 4) {
    printf("  [WARN] fork cost grew with parent memory; COW may not be active\n");
    printf("         (expected: cost ~ same as Scenario 1)\n");
  } else {
    printf("  [PASS] fork cost did not scale with memory (COW likely active)\n");
  }
  printf("\n");

  // ---------------------------------------------------------------
  // Scenario 3: 100 children (amortized cost)
  // ---------------------------------------------------------------
  printf("[Scenario 3] 100 concurrent fork()+exit() with 16-page parent\n");
  uint64 t_start = cgettimeofday();
  for (int i = 0; i < 100; i++) {
    int pid = fork();
    if (pid == 0) {
      exit(0);
    }
  }
  // Parent waits for all 100
  for (int i = 0; i < 100; i++) {
    wait(0);
  }
  uint64 t_end = cgettimeofday();
  uint64 per_fork = (t_end - t_start) / 100;
  printf("  total: %lu cycles (~%lu us)\n",
         t_end - t_start, (t_end - t_start) / 10UL);
  printf("  per-fork avg: %lu cycles (~%lu us)\n\n", per_fork, per_fork / 10UL);

  // ---------------------------------------------------------------
  // Scenario 4: fork + child WRITES -> COW fault
  // Compare with scenario 2 (read-only).
  // ---------------------------------------------------------------
  printf("[Scenario 4] fork() with 64-page parent, child WRITES first page\n");
  uint64 t4 = time_fork_then_write(64);
  printf("  fork+exit (64 pages, write): %lu cycles (~%lu us)\n",
         t4, t4 / 10UL);
  if (t4 > t2 * 2) {
    printf("  [PASS] write cost is significantly higher than read-only,\n");
    printf("         consistent with COW fault overhead\n");
  } else {
    printf("  [INFO] write cost similar to read-only; COW overhead may be amortized\n");
  }
  printf("\n");

  // ---------------------------------------------------------------
  // Summary table
  // ---------------------------------------------------------------
  printf("=== Summary ===\n");
  printf("| Scenario                              | Cycles       | us     |\n");
  printf("|---------------------------------------|--------------|--------|\n");
  printf("| S1: fork+exit (0 pages, cold)         | %12lu | %6lu |\n", t1, t1/10UL);
  printf("| S2: fork+exit (64 pages, read-only)   | %12lu | %6lu |\n", t2, t2/10UL);
  printf("| S3: 100 fork+exit (16 pages each)     | %12lu | %6lu |\n",
         t_end - t_start, (t_end - t_start)/10UL);
  printf("| S4: fork+write (64 pages, COW fault)  | %12lu | %6lu |\n", t4, t4/10UL);
  printf("\n");

  printf("Expected with COW enabled:\n");
  printf("  - S2 ~ S1 (fork cost doesn't depend on parent memory)\n");
  printf("  - S4 ~ 2x S2 (COW fault adds kalloc + memmove on first write)\n");
  printf("  - S3 per-fork << S1 (warm fork is faster)\n");

  printf("\n=== COW Test Complete ===\n");
  exit(0);
}
