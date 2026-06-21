#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char buf[MAXPATH];

  (void)argc;
  (void)argv;

  if (getcwd(buf, sizeof(buf)) < 0) {
    fprintf(2, "pwd: getcwd failed\n");
    exit(1);
  }

  printf("%s\n", buf);
  exit(0);
}
