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

// Sentinel: phys_addr value after freeing (invalid kernel PA, outside 0x80000000-0x88000000)
// This prevents is_shm_pa from recognizing a freed PA as active shm
#define SHM_FREED_PA  0x1

void
shminit(void)
{
  initlock(&shm_lock, "shm");
  for (int i = 0; i < NSHM; i++) {
    shm_table[i].phys_addr = 0;
    shm_table[i].refcount = 0;
    shm_table[i].allocated = 0;
    shm_table[i].key = -1;
    shm_table[i].name[0] = 0;
  }
}

// Find a shared memory segment by key, or return -1 if not found
static int
shm_find_by_key(int key)
{
  for (int i = 0; i < NSHM; i++) {
    if (shm_table[i].allocated && shm_table[i].key == key)
      return i;
  }
  return -1;
}

// Find a free slot in shm_table
static int
shm_find_free(void)
{
  for (int i = 0; i < NSHM; i++) {
    if (!shm_table[i].allocated)
      return i;
  }
  return -1;
}

// Get physical address of a shared memory segment.
// Returns 1 if this PA is actively used by a shm entry (NOT yet freed).
// SHM_FREED_PA is set after kfree so this won't match.
int
is_shm_pa(uint64 pa)
{
  if (pa == 0 || pa >= phys_ram_end || pa < KERNBASE)
    return 0; // not a valid kernel PA
  acquire(&shm_lock);
  for (int i = 0; i < NSHM; i++) {
    if (shm_table[i].allocated && shm_table[i].phys_addr == pa) {
      release(&shm_lock);
      return 1;
    }
  }
  release(&shm_lock);
  return 0;
}

// shmget: create or get a shared memory segment
// key:  segment key (>0 to get existing or create; -1 for unnamed/private)
// size: size in bytes (currently only PGSIZE supported)
// shmflg: IPC_CREAT to create if not exists
// returns segment id (index into shm_table), or -1 on failure
int
shmget(int key, uint64 size, int shmflg)
{
  if (size > SHM_SIZE)
    return -1;

  acquire(&shm_lock);

  // Try to find existing segment
  int idx = -1;
  if (key > 0) {
    idx = shm_find_by_key(key);
    if (idx >= 0) {
      release(&shm_lock);
      return idx;
    }
  }

  // Create new segment
  if (shmflg & 0x200) { // IPC_CREAT
    idx = shm_find_free();
    if (idx < 0) {
      release(&shm_lock);
      return -1; // no free slots
    }

    // Allocate physical page
    char *page = kalloc();
    if (!page) {
      release(&shm_lock);
      return -1;
    }

    shm_table[idx].phys_addr = (uint64)page;
    shm_table[idx].refcount = 0;
    shm_table[idx].allocated = 1;
    shm_table[idx].key = key;
    safestrcpy(shm_table[idx].name, "", sizeof(shm_table[idx].name));
    release(&shm_lock);
    return idx;
  }

  release(&shm_lock);
  return -1;
}

// shmat: attach a shared memory segment to the current process's address space
// key:  segment key (must already exist via shmget)
// addr: (output) receives the user-space virtual address where it is mapped
// returns 0 on success, -1 on failure
int
shmat(int key, uint64 *addr)
{
  int idx;

  // Find the segment
  acquire(&shm_lock);
  idx = shm_find_by_key(key);
  if (idx < 0) {
    release(&shm_lock);
    return -1;
  }
  release(&shm_lock);

  struct proc *p = myproc();
  acquire(&p->lock);

  // Map the physical page at SHM_BASE for this process.
  // Use mappages which allocates intermediate page table pages as needed.
  pte_t *check_pte = walk(p->pagetable, SHM_BASE, 0);
  if (check_pte != 0 && (*check_pte & PTE_V)) {
    // Already mapped (e.g., inherited via fork).
    // Just use the existing mapping, no need to remap.
  } else {
    if (mappages(p->pagetable, SHM_BASE, SHM_SIZE,
                  shm_table[idx].phys_addr, PTE_R | PTE_W | PTE_U) != 0) {
      release(&p->lock);
      return -1;
    }
  }
  // Increment refcount under lock protection
  acquire(&shm_lock);
  shm_table[idx].refcount++;
  release(&shm_lock);
  p->shm_shmidx = idx;
  release(&p->lock);

  *addr = SHM_BASE;
  return 0;
}

// shmdt: detach a shared memory segment from the current process
// addr: the user-space virtual address returned by shmat()
// returns 0 on success, -1 on failure
int
shmdt(uint64 addr)
{
  struct proc *p = myproc();
  acquire(&p->lock);

  if (addr != SHM_BASE) {
    release(&p->lock);
    return -1;
  }

  // Validate that SHM_BASE is actually mapped in this process's page table
  pte_t *pte = walk(p->pagetable, SHM_BASE, 0);
  if (pte == 0 || (*pte & PTE_V) == 0) {
    release(&p->lock);
    return -1;
  }

  uint64 pa = PTE2PA(*pte);

  // Decrement refcount under lock protection.
  // This accounts for the shmat that this process performed.
  // If refcount reaches 0, free the backing page and mark as freed.
  acquire(&shm_lock);
  for (int i = 0; i < NSHM; i++) {
    if (shm_table[i].allocated && shm_table[i].phys_addr == pa) {
      shm_table[i].refcount--;
      if (shm_table[i].refcount <= 0) {
        // Mark as freed with sentinel so is_shm_pa won't match after kfree
        shm_table[i].phys_addr = SHM_FREED_PA;
        shm_table[i].allocated = 0;
        shm_table[i].key = -1;
        shm_table[i].refcount = 0;
        kfree((void *)pa);
      }
      break;
    }
  }
  release(&shm_lock);

  // Unmap from this process's page table.
  // uvmunmap will NOT free shared memory pages (checked by is_shm_pa).
  uvmunmap(p->pagetable, addr, 1, 1);

  // Clear the per-process shm tracking.
  p->shm_shmidx = -1;

  release(&p->lock);
  return 0;
}
