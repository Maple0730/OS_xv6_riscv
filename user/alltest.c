//
// Comprehensive xv6 Feature Test: I/O Redirection + Scheduling
// Runs all tests from a single user program (no need for shell piped input)
//
#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int failed = 0;

void check(int ok, const char *msg)
{
  if (!ok) {
    printf("  [FAIL] %s\n", msg);
    failed++;
  } else {
    printf("  [PASS] %s\n", msg);
  }
}

// ================================================================
// Part 1: I/O Redirection Kernel Tests
// ================================================================
void
test_io_redirection(void)
{
  int fd, r;
  char buf[256];

  printf("\n========== Part 1: I/O Redirection ==========\n");

  // Test 1.1: Create and write a file (simulates echo xxx > file)
  printf("\n[Test 1.1] O_CREATE|O_WRONLY|O_TRUNC (simulates '>')\n");
  unlink("t1.txt");
  fd = open("t1.txt", O_WRONLY | O_CREATE | O_TRUNC);
  check(fd >= 0, "open with O_CREATE|O_TRUNC");
  r = write(fd, "hello world\n", 12);
  check(r == 12, "write 12 bytes");
  close(fd);

  // Test 1.2: Read the file back (simulates cat < file)
  printf("\n[Test 1.2] O_RDONLY (simulates '<')\n");
  fd = open("t1.txt", O_RDONLY);
  check(fd >= 0, "open O_RDONLY");
  memset(buf, 0, sizeof(buf));
  r = read(fd, buf, sizeof(buf));
  check(r == 12, "read 12 bytes");
  check(strcmp(buf, "hello world\n") == 0, "content matches");
  close(fd);

  // Test 1.3: O_APPEND (simulates '>>')
  printf("\n[Test 1.3] O_APPEND (simulates '>>')\n");
  fd = open("t1.txt", O_WRONLY | O_APPEND);
  check(fd >= 0, "open with O_APPEND");
  r = write(fd, "appended line\n", 14);
  check(r == 14, "append 14 bytes");
  close(fd);

  // Verify append
  fd = open("t1.txt", O_RDONLY);
  memset(buf, 0, sizeof(buf));
  r = read(fd, buf, sizeof(buf));
  check(r == 26, "total size is 26 (12+14)");
  check(strcmp(buf, "hello world\nappended line\n") == 0,
        "content has both lines in order");
  close(fd);

  // Test 1.4: O_TRUNC on re-open (simulates '>' overwrite)
  printf("\n[Test 1.4] O_TRUNC overwrite (simulates '>' on existing file)\n");
  fd = open("t1.txt", O_WRONLY | O_CREATE | O_TRUNC);
  check(fd >= 0, "re-open with O_TRUNC");
  r = write(fd, "fresh\n", 6);
  check(r == 6, "write 6 bytes after trunc");
  close(fd);

  fd = open("t1.txt", O_RDONLY);
  memset(buf, 0, sizeof(buf));
  r = read(fd, buf, sizeof(buf));
  check(r == 6, "size is 6 after trunc+write");
  check(strcmp(buf, "fresh\n") == 0, "old content gone");
  close(fd);

  // Test 1.5: Open non-existent file for reading
  printf("\n[Test 1.5] O_RDONLY on non-existent file\n");
  fd = open("no_such_file.txt", O_RDONLY);
  check(fd < 0, "returns error for missing file");

  // Test 1.6: Multiple appends
  printf("\n[Test 1.6] Multiple appends\n");
  unlink("t2.txt");
  fd = open("t2.txt", O_WRONLY | O_CREATE | O_APPEND);
  write(fd, "line1\n", 6);
  close(fd);
  fd = open("t2.txt", O_WRONLY | O_APPEND);
  write(fd, "line2\n", 6);
  close(fd);
  fd = open("t2.txt", O_WRONLY | O_APPEND);
  write(fd, "line3\n", 6);
  close(fd);

  fd = open("t2.txt", O_RDONLY);
  memset(buf, 0, sizeof(buf));
  r = read(fd, buf, sizeof(buf));
  check(r == 18, "3 lines = 18 bytes");
  check(strcmp(buf, "line1\nline2\nline3\n") == 0, "all 3 lines in order");
  close(fd);

  // Cleanup
  unlink("t1.txt");
  unlink("t2.txt");
  printf("\n[I/O Redirection] %d test(s) failed\n", failed);
}

// ================================================================
// Part 2: TFC Scheduling Tests
// ================================================================
void
test_scheduling(void)
{
  int old, r;
  printf("\n========== Part 2: TFC Scheduling ==========\n");

  // Test 2.1: Query current scheduler
  printf("\n[Test 2.1] Query current scheduler\n");
  int cur = sched_algorithm(-1);
  check(cur >= 0 && cur <= 5, "sched_algorithm(-1) returns valid algorithm");
  printf("  Current scheduler: %s (%d)\n", sched_algorithm_name(cur), cur);

  // Test 2.2: Invalid input
  printf("\n[Test 2.2] Invalid algorithm numbers\n");
  r = sched_algorithm(99);
  check(r == -1, "sched_algorithm(99) returns -1");
  r = sched_algorithm(-2);
  check(r == -1, "sched_algorithm(-2) returns -1");

  // Test 2.3: Switch to RR
  printf("\n[Test 2.3] Switch to RR (0)\n");
  old = sched_algorithm(0);
  check(old >= 0, "switch to RR succeeds");
  cur = sched_algorithm(-1);
  check(cur == 0, "current is now RR");

  // Test 2.4: Switch to FCFS
  printf("\n[Test 2.4] Switch to FCFS (1)\n");
  old = sched_algorithm(1);
  check(old == 0, "previous was RR");
  cur = sched_algorithm(-1);
  check(cur == 1, "current is now FCFS");

  // Test 2.5: Switch to MLFQ
  printf("\n[Test 2.5] Switch to MLFQ (2)\n");
  old = sched_algorithm(2);
  check(old == 1, "previous was FCFS");
  cur = sched_algorithm(-1);
  check(cur == 2, "current is now MLFQ");

  // Test 2.6: Switch to SJF (3)
  printf("\n[Test 2.6] Switch to SJF (3)\n");
  old = sched_algorithm(3);
  check(old == 2, "previous was MLFQ");
  cur = sched_algorithm(-1);
  check(cur == 3, "current is now SJF");

  // Test 2.7: Switch to PRIO (4)
  printf("\n[Test 2.7] Switch to PRIO (4)\n");
  old = sched_algorithm(4);
  check(old == 3, "previous was SJF");
  cur = sched_algorithm(-1);
  check(cur == 4, "current is now PRIO");

  // Restore to MLFQ as default
  sched_algorithm(2);

  printf("\n[Scheduling] %d test(s) failed\n", failed);
}

// ================================================================
// Part 3: Shell-level Redirection Tests
// ================================================================
void
test_shell_redirection(void)
{
  int pid, fd, r, status;
  char buf[256];

  printf("\n========== Part 3: Shell-Level Redirection ==========\n");

  // Cleanup first
  unlink("sh_test.txt");
  unlink("sh_out.txt");
  unlink("sh_append.txt");
  unlink("sh_in.txt");
  unlink("comb_in.txt");
  unlink("comb_out.txt");

  // Test 3.1: Output redirection simulation
  // close(1); open("file", O_WRONLY|O_CREATE|O_TRUNC);
  // This is exactly what the shell does for '>'.
  printf("\n[Test 3.1] Output redirection 'echo hello > file'\n");
  pid = fork();
  if (pid == 0) {
    close(1);
    r = open("sh_test.txt", O_WRONLY | O_CREATE | O_TRUNC);
    if (r != 1) exit(1);
    if (write(1, "hello from stdout\n", 18) != 18) exit(1);
    close(1);
    exit(0);
  }
  wait(&status);
  check(status == 0, "child exit cleanly");

  fd = open("sh_test.txt", O_RDONLY);
  memset(buf, 0, sizeof(buf));
  r = read(fd, buf, sizeof(buf));
  check(r == 18, "file has 18 bytes");
  check(strcmp(buf, "hello from stdout\n") == 0, "content correct");
  close(fd);

  // Test 3.2: Input redirection simulation
  // close(0); open("file", O_RDONLY);
  printf("\n[Test 3.2] Input redirection 'cat < file'\n");
  fd = open("sh_in.txt", O_WRONLY | O_CREATE | O_TRUNC);
  write(fd, "input data\n", 11);
  close(fd);

  pid = fork();
  if (pid == 0) {
    close(0);
    r = open("sh_in.txt", O_RDONLY);
    if (r != 0) exit(1);
    memset(buf, 0, sizeof(buf));
    r = read(0, buf, sizeof(buf));
    if (r != 11) exit(1);
    if (strcmp(buf, "input data\n") != 0) exit(1);
    close(0);
    exit(0);
  }
  wait(&status);
  check(status == 0, "child reads from redirected stdin");

  // Test 3.3: Append redirection simulation
  // close(1); open("file", O_WRONLY|O_APPEND);
  printf("\n[Test 3.3] Append redirection 'echo second >> file'\n");
  fd = open("sh_append.txt", O_WRONLY | O_CREATE | O_TRUNC);
  write(fd, "first\n", 6);
  close(fd);

  pid = fork();
  if (pid == 0) {
    close(1);
    r = open("sh_append.txt", O_WRONLY | O_APPEND);
    if (r != 1) exit(1);
    if (write(1, "second\n", 7) != 7) exit(1);
    close(1);
    exit(0);
  }
  wait(&status);
  check(status == 0, "child appends via O_APPEND");

  fd = open("sh_append.txt", O_RDONLY);
  memset(buf, 0, sizeof(buf));
  r = read(fd, buf, sizeof(buf));
  check(r == 13, "file has 13 bytes (6+7)");
  check(strcmp(buf, "first\nsecond\n") == 0, "both lines in order");
  close(fd);

  // Test 3.4: Combined input+output redirection
  // close(0); open(in); close(1); open(out);
  printf("\n[Test 3.4] Combined 'cat < in > out'\n");
  fd = open("comb_in.txt", O_WRONLY | O_CREATE | O_TRUNC);
  write(fd, "combined test\n", 14);
  close(fd);

  pid = fork();
  if (pid == 0) {
    close(0);
    r = open("comb_in.txt", O_RDONLY);
    if (r != 0) exit(1);
    close(1);
    r = open("comb_out.txt", O_WRONLY | O_CREATE | O_TRUNC);
    if (r != 1) exit(1);
    memset(buf, 0, sizeof(buf));
    r = read(0, buf, sizeof(buf));
    if (r != 14) exit(1);
    if (write(1, buf, r) != 14) exit(1);
    close(0);
    close(1);
    exit(0);
  }
  wait(&status);
  check(status == 0, "child combines input+output redirection");

  fd = open("comb_out.txt", O_RDONLY);
  memset(buf, 0, sizeof(buf));
  r = read(fd, buf, sizeof(buf));
  check(r == 14, "output file has 14 bytes");
  check(strcmp(buf, "combined test\n") == 0, "combined redirection works");
  close(fd);

  // Cleanup
  unlink("sh_test.txt");
  unlink("sh_in.txt");
  unlink("sh_append.txt");
  unlink("comb_in.txt");
  unlink("comb_out.txt");

  printf("\n[Shell Redirection] %d test(s) failed\n", failed);
}

int
main(void)
{
  printf("\n");
  printf("╔══════════════════════════════════════════╗\n");
  printf("║  xv6 Feature Verification Test Suite    ║\n");
  printf("║  TFC Scheduling + Maple I/O Redirect    ║\n");
  printf("╚══════════════════════════════════════════╝\n");

  failed = 0;
  test_io_redirection();
  test_scheduling();
  test_shell_redirection();

  printf("\n");
  printf("╔══════════════════════════════════════════╗\n");
  if (failed > 0) {
    printf("║  RESULT: %d TEST(S) FAILED              ║\n", failed);
    printf("╚══════════════════════════════════════════╝\n");
  } else {
    printf("║  RESULT: ALL TESTS PASSED!              ║\n");
    printf("╚══════════════════════════════════════════╝\n");
  }

  return failed > 0 ? 1 : 0;
}
