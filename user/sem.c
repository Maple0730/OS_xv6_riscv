#include "sem.h"
#include "user.h"

// Generic syscall helper using inline assembly
// num: syscall number, a-d: arguments
// Returns value from kernel in a0
static inline long
__syscall(long num, long a, long b, long c, long d)
{
  long ret;
  __asm__ volatile (
    "mv a0, %1\n\t"
    "mv a1, %2\n\t"
    "mv a2, %3\n\t"
    "mv a3, %4\n\t"
    "mv a4, %5\n\t"
    "mv a7, %0\n\t"
    "ecall\n\t"
    "mv %0, a0"
    : "=r"(ret)
    : "r"(a), "r"(b), "r"(c), "r"(d), "r"(num)
    : "a0", "a1", "a2", "a3", "a4", "a5", "a7", "memory"
  );
  return ret;
}

sem_t
sem_open(int value)
{
  long ret = __syscall(SYS_sem_open, value, 0, 0, 0);
  return (ret < 0) ? -1 : (sem_t)ret;
}

int
sem_wait(sem_t sem)
{
  return (int)__syscall(SYS_sem_wait, sem, 0, 0, 0);
}

int
sem_post(sem_t sem)
{
  return (int)__syscall(SYS_sem_post, sem, 0, 0, 0);
}

int
sem_get(sem_t sem, int *value)
{
  long ret = __syscall(SYS_sem_get, sem, (long)value, 0, 0);
  return (int)ret;
}

int
sem_close(sem_t sem)
{
  return (int)__syscall(SYS_sem_close, sem, 0, 0, 0);
}
