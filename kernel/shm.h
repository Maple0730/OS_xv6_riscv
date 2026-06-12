#ifndef SHM_H
#define SHM_H

#include "types.h"

#define NSHM 16
#define SHM_BASE 0x3FEFE000

void shminit(void);
int shmget(int key, uint64 *addr);
int shmat(int key, uint64 *addr);
int shmdt(uint64 addr);
int is_shm_pa(uint64 pa);

#endif
