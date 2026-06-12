#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "sem.h"
#include "defs.h"

struct semaphore semtable[NSEM];

// Initialize the semaphore table
void
seminit(void)
{
  for (int i = 0; i < NSEM; i++) {
    initlock(&semtable[i].lock, "semaphore");
    semtable[i].value = 0;
    semtable[i].allocated = 0;
    semtable[i].waiters = 0;
    semtable[i].name[0] = 0;
  }
}

// Find a free semaphore slot
static int
sem_alloc(void)
{
  for (int i = 0; i < NSEM; i++) {
    if (!semtable[i].allocated) {
      return i;
    }
  }
  return -1;
}

// Initialize a semaphore with a given value
int
sem_init(int sem_id, int value)
{
  if (sem_id < 0 || sem_id >= NSEM) {
    return -1;
  }

  struct semaphore *sem = &semtable[sem_id];
  acquire(&sem->lock);
  sem->value = value;
  sem->allocated = 1;
  sem->waiters = 0;
  release(&sem->lock);

  return 0;
}

// Open (allocate) a new semaphore with initial value
int
sem_open(int *sem_id, int value)
{
  int id = sem_alloc();
  if (id < 0) {
    return -1;  // No free semaphore slots
  }

  *sem_id = id;
  return sem_init(id, value);
}

// P operation: decrement semaphore, block if <= 0
int
sem_wait(int sem_id)
{
  if (sem_id < 0 || sem_id >= NSEM) {
    return -1;
  }

  struct semaphore *sem = &semtable[sem_id];

  acquire(&sem->lock);

  if (!sem->allocated) {
    release(&sem->lock);
    return -1;
  }

  sem->value--;
  if (sem->value < 0) {
    // Must sleep to wait for V operation
    printf("[sem_wait] pid=%d sem=%d value=%d -> sleeping\n",
           myproc()->pid, sem_id, sem->value + 1);
    sleep(sem, &sem->lock);
    printf("[sem_wait] pid=%d sem=%d -> woke up\n",
           myproc()->pid, sem_id);
  }

  release(&sem->lock);
  return 0;
}

// V operation: increment semaphore, wake up a waiter if any
int
sem_post(int sem_id)
{
  if (sem_id < 0 || sem_id >= NSEM) {
    return -1;
  }

  struct semaphore *sem = &semtable[sem_id];

  acquire(&sem->lock);

  if (!sem->allocated) {
    release(&sem->lock);
    return -1;
  }

  printf("[sem_post] pid=%d sem=%d value=%d\n",
         myproc()->pid, sem_id, sem->value);
  sem->value++;
  if (sem->value <= 0) {
    // There are waiters, wake one up
    printf("[sem_post] pid=%d sem=%d -> waking up waiter\n",
           myproc()->pid, sem_id);
    wakeup(sem);
  }

  release(&sem->lock);

  return 0;
}

// Get the current value of a semaphore
int
sem_get(int sem_id, int *value)
{
  if (sem_id < 0 || sem_id >= NSEM) {
    return -1;
  }

  struct semaphore *sem = &semtable[sem_id];

  acquire(&sem->lock);
  if (!sem->allocated) {
    release(&sem->lock);
    return -1;
  }
  *value = sem->value;
  release(&sem->lock);

  return 0;
}

// Close (deallocate) a semaphore
int
sem_close(int sem_id)
{
  if (sem_id < 0 || sem_id >= NSEM) {
    return -1;
  }

  struct semaphore *sem = &semtable[sem_id];

  acquire(&sem->lock);
  if (!sem->allocated) {
    release(&sem->lock);
    return -1;
  }

  // Wake up all waiters
  while (sem->waiters != 0 || sem->value < 0) {
    wakeup(sem);
  }

  sem->allocated = 0;
  sem->value = 0;
  sem->waiters = 0;
  release(&sem->lock);

  return 0;
}
