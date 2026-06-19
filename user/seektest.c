#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user/user.h"

static void
fail(char *msg)
{
  fprintf(2, "seektest: %s\n", msg);
  exit(1);
}

int
main(void)
{
  int fd, dupfd, pfd[2];
  int n;
  char buf[8];

  unlink("seektmp");
  fd = open("seektmp", O_CREATE | O_TRUNC | O_RDWR);
  if (fd < 0)
    fail("open");

  if (write(fd, "abcdef", 6) != 6)
    fail("initial write");

  if (lseek(fd, 2, SEEK_SET) != 2)
    fail("seek set");
  memset(buf, 0, sizeof(buf));
  n = read(fd, buf, 2);
  if (n != 2 || buf[0] != 'c' || buf[1] != 'd')
    fail("read after seek set");

  if (lseek(fd, -1, SEEK_CUR) != 3)
    fail("seek cur");
  memset(buf, 0, sizeof(buf));
  n = read(fd, buf, 1);
  if (n != 1 || buf[0] != 'd')
    fail("read after seek cur");

  if (lseek(fd, -2, SEEK_END) != 4)
    fail("seek end");
  memset(buf, 0, sizeof(buf));
  n = read(fd, buf, 2);
  if (n != 2 || buf[0] != 'e' || buf[1] != 'f')
    fail("read after seek end");

  if (lseek(fd, 3, SEEK_SET) != 3)
    fail("seek for overwrite");
  if (write(fd, "XY", 2) != 2)
    fail("overwrite write");
  if (lseek(fd, 0, SEEK_SET) != 0)
    fail("seek rewind");
  memset(buf, 0, sizeof(buf));
  n = read(fd, buf, 6);
  if (n != 6 || memcmp(buf, "abcXYf", 6) != 0)
    fail("overwrite verify");

  dupfd = dup(fd);
  if (dupfd < 0)
    fail("dup");
  if (lseek(dupfd, 4, SEEK_SET) != 4)
    fail("dup seek");
  memset(buf, 0, sizeof(buf));
  n = read(fd, buf, 1);
  if (n != 1 || buf[0] != 'Y')
    fail("shared offset");
  close(dupfd);

  if (lseek(fd, 0, SEEK_END) != 6)
    fail("seek to eof");
  memset(buf, 0, sizeof(buf));
  n = read(fd, buf, 1);
  if (n != 0)
    fail("read at eof");

  if (lseek(fd, 2, SEEK_END) != -1)
    fail("seek past eof should fail");

  if (lseek(fd, -1, SEEK_SET) != -1)
    fail("negative seek should fail");
  if (lseek(fd, 0, 99) != -1)
    fail("bad whence should fail");

  if (pipe(pfd) < 0)
    fail("pipe");
  if (lseek(pfd[0], 0, SEEK_SET) != -1)
    fail("pipe seek should fail");
  close(pfd[0]);
  close(pfd[1]);

  close(fd);
  unlink("seektmp");
  printf("seektest: ok\n");
  exit(0);
}
