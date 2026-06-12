#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "shm.h"
#include "defs.h"

struct shm shm_table[NSHM];
struct spinlock shm_lock;

void
shminit(void)
{
  initlock(&shm_lock, "shm");
  for (int i = 0; i < NSHM; i++) {
    shm_table[i].addr = 0;
    shm_table[i].phys_addr = 0;
    shm_table[i].refcount = 0;
    shm_table[i].key = -1;
  }
}

// Check if a physical address is a shared memory address
int
is_shm_pa(uint64 pa)
{
  return 0;  // Shared memory disabled
}
