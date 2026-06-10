#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  printf("shutting down...\n");
  halt();
  exit(0);
}
