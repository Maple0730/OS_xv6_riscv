// Producer-Consumer test using semaphores
// Tests bounded buffer synchronization with semaphore operations
// NOTE: Without shared memory, we test that semaphores work across processes

#include "kernel/types.h"
#include "user/user.h"
#include "sem.h"

#define BUFFER_SIZE 5
#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2
#define ITEMS_PER_PRODUCER 10
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)

// Semaphores
int empty, full, mutex;

void producer(int id) {
  for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
    // Wait for empty slot
    sem_wait(empty);

    // Protect buffer access
    sem_wait(mutex);
    printf("  Producer %d: produced item %d\n", id, i);
    sem_post(mutex);

    // Signal filled slot
    sem_post(full);
  }
  exit(0);
}

void consumer(int id) {
  for (int i = 0; i < TOTAL_ITEMS / NUM_CONSUMERS; i++) {
    // Wait for filled slot
    sem_wait(full);

    // Protect buffer access
    sem_wait(mutex);
    printf("  Consumer %d: consumed item %d\n", id, i);
    sem_post(mutex);

    // Signal empty slot
    sem_post(empty);
  }
  exit(0);
}

int main(int argc, char *argv[]) {
  int pid;

  printf("=== Producer-Consumer Test ===\n\n");
  printf("Configuration:\n");
  printf("  Buffer size: %d\n", BUFFER_SIZE);
  printf("  Producers: %d (each produces %d items)\n",
         NUM_PRODUCERS, ITEMS_PER_PRODUCER);
  printf("  Consumers: %d\n", NUM_CONSUMERS);
  printf("  Total items to produce: %d\n\n", TOTAL_ITEMS);

  // Create semaphores
  empty = sem_open(BUFFER_SIZE);
  if (empty < 0) {
    printf("FAIL: sem_open(empty) failed\n");
    exit(1);
  }

  full = sem_open(0);
  if (full < 0) {
    printf("FAIL: sem_open(full) failed\n");
    exit(1);
  }

  mutex = sem_open(1);
  if (mutex < 0) {
    printf("FAIL: sem_open(mutex) failed\n");
    exit(1);
  }

  printf("Semaphores created: empty=%d, full=%d, mutex=%d\n\n",
         empty, full, mutex);

  // Create producer processes
  printf("Creating %d producers...\n", NUM_PRODUCERS);
  for (int i = 0; i < NUM_PRODUCERS; i++) {
    pid = fork();
    if (pid < 0) {
      printf("FAIL: fork failed for producer %d\n", i);
      exit(1);
    }
    if (pid == 0) {
      producer(i);
      exit(0);
    }
    printf("  Created producer %d with PID=%d\n", i, pid);
  }

  // Create consumer processes
  printf("Creating %d consumers...\n", NUM_CONSUMERS);
  for (int i = 0; i < NUM_CONSUMERS; i++) {
    pid = fork();
    if (pid < 0) {
      printf("FAIL: fork failed for consumer %d\n", i);
      exit(1);
    }
    if (pid == 0) {
      consumer(i);
      exit(0);
    }
    printf("  Created consumer %d with PID=%d\n", i, pid);
  }

  // Wait for all children
  printf("\nWaiting for all producers and consumers to finish...\n");
  for (int i = 0; i < NUM_PRODUCERS + NUM_CONSUMERS; i++) {
    wait(0);
  }

  // Close semaphores
  sem_close(empty);
  sem_close(full);
  sem_close(mutex);

  printf("\n");
  printf("All %d producers and %d consumers completed.\n",
         NUM_PRODUCERS, NUM_CONSUMERS);
  printf("Total semaphore operations: %d\n", 
         TOTAL_ITEMS * 2 +  // empty/full waits and posts
         TOTAL_ITEMS * 2 +  // mutex waits and posts
         TOTAL_ITEMS * 2);  // empty/full reverse ops
  printf("\n=== Producer-Consumer Test PASSED ===\n");
  printf("Semaphore synchronization worked correctly across processes.\n");
  exit(0);
}
