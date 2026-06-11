// lseek test: verify SEEK_SET, SEEK_CUR, SEEK_END
#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main(void)
{
  int fd, n;
  char buf[32];

  // Step 1: create a test file
  fd = open("seektest.txt", O_RDWR | O_CREATE);
  if (fd < 0) {
    printf("lseektest: cannot create file\n");
    exit(1);
  }
  write(fd, "0123456789ABCDEF", 16);  // 16 bytes
  close(fd);

  printf("lseektest: file created, 16 bytes\n");

  // Step 2: reopen and test SEEK_SET
  fd = open("seektest.txt", O_RDONLY);
  if (fd < 0) {
    printf("lseektest: cannot open file\n");
    exit(1);
  }

  // seek to offset 10 ("ABCDEF"), read 6 bytes
  n = lseek(fd, 10, SEEK_SET);
  printf("lseek(10, SEEK_SET) = %d\n", n);

  n = read(fd, buf, 6);
  buf[n] = 0;
  printf("read 6 bytes: \"%s\" (expect \"ABCDEF\")\n", buf);

  // Step 3: test SEEK_CUR — move forward 2 from current pos (16)
  // current pos is 10+6=16, seek back -6 to re-read
  n = lseek(fd, -6, SEEK_CUR);
  printf("lseek(-6, SEEK_CUR) = %d\n", n);

  n = read(fd, buf, 6);
  buf[n] = 0;
  printf("read 6 bytes: \"%s\" (expect \"ABCDEF\")\n", buf);

  // Step 4: test SEEK_END — from end, back 6 bytes
  n = lseek(fd, -6, SEEK_END);
  printf("lseek(-6, SEEK_END) = %d\n", n);

  n = read(fd, buf, 6);
  buf[n] = 0;
  printf("read 6 bytes: \"%s\" (expect \"ABCDEF\")\n", buf);

  // Step 5: SEEK_END with offset 0 gives file size
  n = lseek(fd, 0, SEEK_END);
  printf("lseek(0, SEEK_END) = %d (file size, expect 16)\n", n);

  close(fd);

  // cleanup
  unlink("seektest.txt");

  printf("lseektest: all checks passed\n");
  exit(0);
}
