// Basic semaphore test
// Tests basic P/V operations

#include "kernel/types.h"
#include "user/user.h"
#include "sem.h"

#define SEM_VALUE 1

int main(int argc, char *argv[]) {
  int sem_id;
  int value;
  int pid;

  printf("=== Semaphore Basic Test ===\n\n");

  // Test 1: Create semaphore
  printf("Test 1: Create semaphore with value=%d\n", SEM_VALUE);
  sem_id = sem_open(SEM_VALUE);
  if (sem_id < 0) {
    printf("FAIL: sem_open failed\n");
    exit(1);
  }
  printf("PASS: sem_open returned sem_id=%d\n\n", sem_id);

  // Test 2: Get initial value
  printf("Test 2: Get initial value\n");
  if (sem_get(sem_id, &value) < 0) {
    printf("FAIL: sem_get failed\n");
    exit(1);
  }
  if (value != SEM_VALUE) {
    printf("FAIL: expected value=%d, got %d\n", SEM_VALUE, value);
    exit(1);
  }
  printf("PASS: initial value=%d\n\n", value);

  // Test 3: P operation (decrement)
  printf("Test 3: sem_wait (P operation)\n");
  if (sem_wait(sem_id) < 0) {
    printf("FAIL: sem_wait failed\n");
    exit(1);
  }
  if (sem_get(sem_id, &value) < 0) {
    printf("FAIL: sem_get failed\n");
    exit(1);
  }
  if (value != 0) {
    printf("FAIL: expected value=0 after wait, got %d\n", value);
    exit(1);
  }
  printf("PASS: value after wait=%d\n\n", value);

  // Test 4: Fork a child to do V operation
  printf("Test 4: Fork child to do sem_post (V operation)\n");
  pid = fork();
  if (pid < 0) {
    printf("FAIL: fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // Child: signal the semaphore after a short delay
    pause(1);
    printf("  Child (PID=%d): doing sem_post\n", getpid());
    if (sem_post(sem_id) < 0) {
      printf("  Child: sem_post failed\n");
      exit(1);
    }
    printf("  Child: sem_post done\n");
    exit(0);
  }

  // Parent: try to wait again (should block and then be woken up)
  printf("  Parent (PID=%d): doing sem_wait (will block)\n", getpid());
  int start = uptime();
  if (sem_wait(sem_id) < 0) {
    printf("FAIL: sem_wait failed\n");
    exit(1);
  }
  int end = uptime();
  printf("  Parent: woke up after %d ticks\n", end - start);

  // Wait for child
  wait(0);

  // Test 5: Close semaphore
  printf("\nTest 5: Close semaphore\n");
  if (sem_close(sem_id) < 0) {
    printf("FAIL: sem_close failed\n");
    exit(1);
  }
  printf("PASS: sem_close succeeded\n");

  printf("\n=== All Basic Tests PASSED ===\n");
  exit(0);
}
