#ifndef XV6_SEMAPHORE_H
#define XV6_SEMAPHORE_H

#define NSEM 16  // Maximum number of semaphores

struct proc;

// Waiter structure for semaphore queue
struct sem_waiter {
  struct proc *proc;
  struct sem_waiter *next;
};

// Semaphore structure
struct semaphore {
  struct spinlock lock;       // Protects the semaphore structure
  int value;                  // Semaphore value
  int allocated;              // Whether this semaphore is allocated
  char name[16];              // Name for debugging
  struct sem_waiter *waiters; // List of waiting processes
};

// Global semaphore table
extern struct semaphore semtable[NSEM];

// Kernel API functions
int sem_init(int sem_id, int value);
int sem_open(int *sem_id, int value);
int sem_wait(int sem_id);
int sem_post(int sem_id);
int sem_get(int sem_id, int *value);
int sem_close(int sem_id);

#endif // XV6_SEMAPHORE_H
