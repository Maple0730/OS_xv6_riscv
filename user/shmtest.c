// Shared Memory Test Program
// Tests: shmget/shmat/shmdt, parent-child IPC, fork inheritance

#include "kernel/types.h"
#include "user/user.h"

#define SHM_KEY    0x1234
#define SHM_SIZE   4096

// Test 1: Basic shared memory creation and attachment
void test_basic(void) {
  printf("\n=== Test 1: Basic shmget/shmat/shmdt ===\n");

  uint64 addr;
  int shmid = shmget(SHM_KEY, SHM_SIZE, 0x200); // IPC_CREAT
  if (shmid < 0) {
    printf("  FAIL: shmget failed\n");
    return;
  }
  printf("  shmget returned shmid=%d\n", shmid);

  int ret = shmat(SHM_KEY, &addr);
  if (ret < 0) {
    printf("  FAIL: shmat failed\n");
    return;
  }
  printf("  shmat returned addr=0x%x\n", (int)addr);

  // Write to shared memory
  int *data = (int *)addr;
  data[0] = 42;
  data[1] = 100;
  printf("  Wrote: data[0]=%d, data[1]=%d\n", data[0], data[1]);

  // Read back
  printf("  Read:  data[0]=%d, data[1]=%d\n", data[0], data[1]);

  if (data[0] == 42 && data[1] == 100)
    printf("  PASS\n");
  else
    printf("  FAIL: data mismatch\n");

  // Detach
  ret = shmdt(addr);
  if (ret < 0)
    printf("  FAIL: shmdt failed\n");
  else
    printf("  shmdt succeeded\n");
}

// Test 2: Parent-child communication via shared memory
void test_parent_child(void) {
  printf("\n=== Test 2: Parent-child IPC via shared memory ===\n");

  uint64 addr;
  int shmid = shmget(SHM_KEY + 1, SHM_SIZE, 0x200);
  if (shmid < 0) {
    printf("  FAIL: shmget failed\n");
    return;
  }
  printf("  shmid=%d\n", shmid);

  int ret = shmat(SHM_KEY + 1, &addr);
  if (ret < 0) {
    printf("  FAIL: shmat failed\n");
    return;
  }
  printf("  Parent: addr=0x%x\n", (int)addr);

  int *data = (int *)addr;
  data[0] = 0;  // flag: 0=not ready, 1=ready, 2=done
  data[1] = 0;  // value

  int pid = fork();
  if (pid < 0) {
    printf("  FAIL: fork failed\n");
    shmdt(addr);
    return;
  }

  if (pid == 0) {
    // Child: attach to same segment
    uint64 child_addr;
    if (shmat(SHM_KEY + 1, &child_addr) < 0) {
      printf("  Child: shmat FAILED\n");
      exit(1);
    }
    printf("  Child: addr=0x%x\n", (int)child_addr);

    int *cdata = (int *)child_addr;
    // Wait for parent to write
    while (cdata[0] == 0)
      pause(1);
    printf("  Child: read data[1]=%d (expected 999)\n", cdata[1]);
    if (cdata[1] == 999) {
      cdata[0] = 2; // done
      printf("  Child: PASS\n");
    } else {
      printf("  Child: FAIL\n");
    }
    shmdt(child_addr);
    exit(0);
  } else {
    // Parent: write data for child
    pause(5);
    data[0] = 1;  // signal ready
    data[1] = 999; // actual data
    printf("  Parent: wrote data[0]=1, data[1]=999\n");

    // Wait for child to finish
    while (data[0] != 2)
      pause(1);
    printf("  Parent: child finished\n");
  }

  shmdt(addr);
}

// Test 3: Fork inheritance - child inherits shared memory mapping
void test_fork_inheritance(void) {
  printf("\n=== Test 3: Fork inheritance of shared memory ===\n");

  uint64 addr;
  int shmid = shmget(SHM_KEY + 2, SHM_SIZE, 0x200);
  if (shmid < 0) {
    printf("  FAIL: shmget failed\n");
    return;
  }

  int ret = shmat(SHM_KEY + 2, &addr);
  if (ret < 0) {
    printf("  FAIL: shmat failed\n");
    return;
  }

  int *data = (int *)addr;
  data[0] = 12345;
  printf("  Parent wrote: data[0]=%d\n", data[0]);

  int pid = fork();
  if (pid < 0) {
    printf("  FAIL: fork failed\n");
    shmdt(addr);
    return;
  }

  if (pid == 0) {
    // Child: read from inherited shared memory
    uint64 child_addr;
    if (shmat(SHM_KEY + 2, &child_addr) < 0) {
      printf("  Child: shmat FAILED\n");
      exit(1);
    }
    int *cdata = (int *)child_addr;
    printf("  Child read: data[0]=%d (expected 12345)\n", cdata[0]);
    if (cdata[0] == 12345) {
      cdata[0] = 67890;  // modify
      printf("  Child: PASS, wrote data[0]=%d\n", cdata[0]);
    } else {
      printf("  Child: FAIL\n");
    }
    shmdt(child_addr);
    exit(0);
  } else {
    // Wait for child
    wait(0);
    printf("  Parent read: data[0]=%d (expected 67890 after child modification)\n", data[0]);
    if (data[0] == 67890)
      printf("  PASS\n");
    else
      printf("  FAIL: data not shared\n");
  }

  shmdt(addr);
}

int main(int argc, char *argv[]) {
  printf("=== Shared Memory Test Suite ===\n");
  printf("SHM_BASE=0x%x, SHM_SIZE=%d\n", 0x3FEFE000, SHM_SIZE);

  test_basic();
  test_parent_child();
  test_fork_inheritance();

  printf("\n=== All tests complete ===\n");
  return 0;
}
