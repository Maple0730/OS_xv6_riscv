#ifndef SHM_H
#define SHM_H

#include "types.h"

#define NSHM      16       // maximum shared memory segments
#define SHM_BASE  0x3FEFE000  // user-space address for shared memory attachment
#define SHM_SIZE  0x1000   // one page (4096 bytes) per segment

struct shm {
  char name[32];          // shared memory segment name (unused for now)
  uint64 phys_addr;       // physical address of the backing page
  int refcount;           // number of processes that have mapped this (shmat + kfork)
  int allocated;           // whether this slot is in use
  int key;                // user-visible key (-1 if unnamed)
};

void shminit(void);
int shmget(int key, uint64 size, int shmflg);
int shmat(int key, uint64 *addr);
int shmdt(uint64 addr);
int is_shm_pa(uint64 pa);

#endif
