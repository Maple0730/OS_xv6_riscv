//
// I/O Redirection Test - tests file operations for redirection support
// Tests: O_APPEND, O_TRUNC, O_CREATE, file read/write
//
#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "user/user.h"

#define TEST_FILE  "iotest.txt"
#define TEST_FILE2 "iotest2.txt"

int failed = 0;

void
check(int ok, const char *msg)
{
  if (!ok) {
    printf("  FAIL: %s\n", msg);
    failed++;
  } else {
    printf("  PASS: %s\n", msg);
  }
}

int
main(void)
{
  int fd, r;
  char buf[128];

  printf("=== I/O Redirection Kernel Test ===\n\n");

  // Clean up any leftover test files
  unlink(TEST_FILE);
  unlink(TEST_FILE2);

  // ============================================================
  // Test 1: O_CREATE | O_WRONLY — create and write a file
  // ============================================================
  printf("[Test 1] Create file with O_CREATE|O_WRONLY|O_TRUNC\n");
  fd = open(TEST_FILE, O_WRONLY | O_CREATE | O_TRUNC);
  check(fd >= 0, "open(create) returns valid fd");

  r = write(fd, "hello world\n", 12);
  check(r == 12, "write 12 bytes to new file");
  close(fd);

  // ============================================================
  // Test 2: O_RDONLY — read back the file
  // ============================================================
  printf("\n[Test 2] Read file with O_RDONLY\n");
  fd = open(TEST_FILE, O_RDONLY);
  check(fd >= 0, "open(read) returns valid fd");

  memset(buf, 0, sizeof(buf));
  r = read(fd, buf, sizeof(buf));
  check(r == 12, "read returns 12 bytes");
  check(strcmp(buf, "hello world\n") == 0, "content matches 'hello world\\n'");
  close(fd);

  // ============================================================
  // Test 3: O_APPEND — append to existing file
  // ============================================================
  printf("\n[Test 3] Append to file with O_APPEND\n");
  fd = open(TEST_FILE, O_WRONLY | O_APPEND);
  check(fd >= 0, "open(append) returns valid fd");

  r = write(fd, "second line\n", 12);
  check(r == 12, "write 12 bytes with O_APPEND");
  close(fd);

  // Verify append worked correctly
  fd = open(TEST_FILE, O_RDONLY);
  memset(buf, 0, sizeof(buf));
  r = read(fd, buf, sizeof(buf));
  check(r == 24, "read returns 24 bytes (12 + 12)");
  check(strcmp(buf, "hello world\nsecond line\n") == 0,
        "content matches 'hello world\\nsecond line\\n'");
  close(fd);

  // ============================================================
  // Test 4: O_TRUNC — truncate on open
  // ============================================================
  printf("\n[Test 4] Truncate file with O_TRUNC\n");
  fd = open(TEST_FILE, O_WRONLY | O_CREATE | O_TRUNC);
  check(fd >= 0, "open(trunc) returns valid fd");

  r = write(fd, "fresh\n", 6);
  check(r == 6, "write 6 bytes after truncate");
  close(fd);

  // Verify truncation
  fd = open(TEST_FILE, O_RDONLY);
  memset(buf, 0, sizeof(buf));
  r = read(fd, buf, sizeof(buf));
  check(r == 6, "read returns 6 bytes after truncate");
  check(strcmp(buf, "fresh\n") == 0, "content is 'fresh\\n'");
  close(fd);

  // ============================================================
  // Test 5: O_APPEND creates new file with O_CREATE
  // ============================================================
  printf("\n[Test 5] O_APPEND with O_CREATE on new file\n");
  unlink(TEST_FILE2);
  fd = open(TEST_FILE2, O_WRONLY | O_CREATE | O_APPEND);
  check(fd >= 0, "open(create+append) returns valid fd");

  r = write(fd, "first append\n", 13);
  check(r == 13, "write 13 bytes to new append file");
  close(fd);

  fd = open(TEST_FILE2, O_WRONLY | O_APPEND);
  r = write(fd, "second append\n", 14);
  check(r == 14, "append 14 more bytes");
  close(fd);

  fd = open(TEST_FILE2, O_RDONLY);
  memset(buf, 0, sizeof(buf));
  r = read(fd, buf, sizeof(buf));
  check(r == 27, "read returns 27 bytes (13+14)");
  check(strcmp(buf, "first append\nsecond append\n") == 0,
        "content matches expected");
  close(fd);

  // ============================================================
  // Test 6: O_RDONLY on non-existent file should fail
  // ============================================================
  printf("\n[Test 6] Open non-existent file for reading\n");
  fd = open("nonexistent_file.txt", O_RDONLY);
  check(fd < 0, "open(nonexistent) returns error");

  // ============================================================
  // Clean up
  // ============================================================
  unlink(TEST_FILE);
  unlink(TEST_FILE2);

  printf("\n=== Results: %d test(s) failed ===\n", failed);
  if (failed > 0)
    printf("SOME TESTS FAILED!\n");
  else
    printf("All tests passed!\n");

  return failed;
}
