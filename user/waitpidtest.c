#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void test_wait_any(void);
void test_wait_specific(void);
void test_wait_multiple(void);
void test_wait_invalid_pid(void);

int main(int argc, char *argv[])
{
  printf("=== waitpid Test Suite ===\n\n");

  printf("Test 1: waitpid(-1, &status) - wait for any child\n");
  test_wait_any();
  printf("\n");

  printf("Test 2: waitpid(specific_pid, &status) - wait for specific child\n");
  test_wait_specific();
  printf("\n");

  printf("Test 3: waitpid(-1, &status) - wait for multiple children\n");
  test_wait_multiple();
  printf("\n");

  printf("Test 4: waitpid(invalid_pid, &status) - wait for non-existent child\n");
  test_wait_invalid_pid();
  printf("\n");

  printf("=== All tests completed ===\n");
  exit(0);
}

void test_wait_any(void)
{
  int pid = fork();
  if (pid < 0) {
    printf("ERROR: fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // Child: exit with status 42
    exit(42);
  }

  // Parent: wait for any child
  int status;
  int ret = waitpid(-1, &status);
  if (ret < 0) {
    printf("ERROR: waitpid(-1) failed\n");
    exit(1);
  }
  if (ret != pid) {
    printf("ERROR: waitpid returned wrong pid: expected %d, got %d\n", pid, ret);
    exit(1);
  }
  if (status != 42) {
    printf("ERROR: waitpid returned wrong status: expected 42, got %d\n", status);
    exit(1);
  }
  printf("PASSED: waitpid(-1, &status) returned pid=%d, status=%d\n", ret, status);
}

void test_wait_specific(void)
{
  int pid1 = fork();
  if (pid1 < 0) {
    printf("ERROR: fork failed\n");
    exit(1);
  }

  if (pid1 == 0) {
    // First child: sleep then exit
    pause(10);
    exit(100);
  }

  int pid2 = fork();
  if (pid2 < 0) {
    printf("ERROR: fork failed\n");
    exit(1);
  }

  if (pid2 == 0) {
    // Second child: exit immediately
    exit(200);
  }

  // Wait for first child specifically (should block until it exits)
  int status;
  int ret = waitpid(pid1, &status);
  if (ret < 0) {
    printf("ERROR: waitpid(pid1) failed\n");
    exit(1);
  }
  if (ret != pid1) {
    printf("ERROR: waitpid returned wrong pid: expected %d, got %d\n", pid1, ret);
    exit(1);
  }
  if (status != 100) {
    printf("ERROR: waitpid returned wrong status: expected 100, got %d\n", status);
    exit(1);
  }
  printf("PASSED: waitpid(%d, &status) returned pid=%d, status=%d\n", pid1, ret, status);

  // Now wait for second child
  ret = waitpid(pid2, &status);
  if (ret < 0) {
    printf("ERROR: waitpid(pid2) failed\n");
    exit(1);
  }
  if (ret != pid2) {
    printf("ERROR: waitpid returned wrong pid: expected %d, got %d\n", pid2, ret);
    exit(1);
  }
  if (status != 200) {
    printf("ERROR: waitpid returned wrong status: expected 200, got %d\n", status);
    exit(1);
  }
  printf("PASSED: waitpid(%d, &status) returned pid=%d, status=%d\n", pid2, ret, status);
}

void test_wait_multiple(void)
{
  int pids[3];
  int i;

  // Create 3 children
  for (i = 0; i < 3; i++) {
    pids[i] = fork();
    if (pids[i] < 0) {
      printf("ERROR: fork failed\n");
      exit(1);
    }
    if (pids[i] == 0) {
      // Children exit with different statuses
      exit(10 + i);
    }
  }

  // Wait for all children using waitpid(-1, &status)
  int collected = 0;
  int status;
  while (collected < 3) {
    int ret = waitpid(-1, &status);
    if (ret < 0) {
      printf("ERROR: waitpid(-1) failed\n");
      exit(1);
    }
    printf("Collected child: pid=%d, status=%d\n", ret, status);
    collected++;
  }
  printf("PASSED: All 3 children collected with waitpid(-1, &status)\n");
}

void test_wait_invalid_pid(void)
{
  // Try to wait for a non-existent child
  int status;
  int ret = waitpid(9999, &status);
  if (ret != -1) {
    printf("ERROR: waitpid(9999) should return -1 for non-existent child\n");
    exit(1);
  }
  printf("PASSED: waitpid(9999, &status) correctly returned -1 (no such child)\n");
}
