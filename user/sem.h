#ifndef XV6_USER_SEM_H
#define XV6_USER_SEM_H

// Semaphore handle type
typedef int sem_t;

// Initialize/open a semaphore with given value
// Returns semaphore id (>= 0) on success, -1 on failure
sem_t sem_open(int value);

// P operation: wait/decrement
// Blocks if semaphore value is <= 0
// Returns 0 on success, -1 on failure
int sem_wait(sem_t sem);

// V operation: signal/increment
// Wakes up one waiting process if any
// Returns 0 on success, -1 on failure
int sem_post(sem_t sem);

// Get current semaphore value
// Returns 0 on success, -1 on failure
int sem_get(sem_t sem, int *value);

// Close/release a semaphore
// Returns 0 on success, -1 on failure
int sem_close(sem_t sem);

#endif // XV6_USER_SEM_H
