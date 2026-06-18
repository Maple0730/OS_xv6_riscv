#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if (addr >= p->sz ||
      addr + sizeof(uint64) > p->sz) // both tests needed, in case of overflow
    return -1;
  if (copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  if (copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
void
argint(int n, int *ip)
{
  *ip = argraw(n);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (not including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_pause(void);
extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
extern uint64 sys_ps(void);
extern uint64 sys_halt(void);
extern uint64 sys_lseek(void);
extern uint64 sys_sem_open(void);
extern uint64 sys_sem_wait(void);
extern uint64 sys_sem_post(void);
extern uint64 sys_sem_get(void);
extern uint64 sys_sem_close(void);
extern uint64 sys_shmget(void);
extern uint64 sys_shmat(void);
extern uint64 sys_shmdt(void);
// merge-0617 socket syscalls
extern uint64 sys_socket(void);
extern uint64 sys_bind(void);
extern uint64 sys_sendto(void);
extern uint64 sys_recvfrom(void);
// tfc extended syscalls
extern uint64 sys_waitpid(void);
extern uint64 sys_sched_algorithm(void);
extern uint64 sys_settimeslice(void);
extern uint64 sys_gettimeslice(void);
extern uint64 sys_cgettimeofday(void);
extern uint64 sys_schedstat(void);
extern uint64 sys_sched_setburst(void);
extern uint64 sys_banker_init(void);
extern uint64 sys_banker_setmax(void);
extern uint64 sys_banker_request(void);
extern uint64 sys_banker_release(void);
extern uint64 sys_banker_safe_sequence(void);
extern uint64 sys_banker_get_state(void);
extern uint64 sys_banker_setmax_alloc(void);
extern uint64 sys_mon_create(void);
extern uint64 sys_mon_lock(void);
extern uint64 sys_mon_unlock(void);
extern uint64 sys_mon_wait(void);
extern uint64 sys_mon_signal(void);
extern uint64 sys_mon_broadcast(void);
extern uint64 sys_deadlock_set(void);
extern uint64 sys_setpriority(void);
extern uint64 sys_getpriority(void);
extern uint64 sys_rt_register(void);
extern uint64 sys_rt_wait_period(void);
extern uint64 sys_getcpuid(void);
extern uint64 sys_setcpuaffinity(void);
extern uint64 sys_msgget(void);
extern uint64 sys_msgsnd(void);
extern uint64 sys_msgrcv(void);

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
  // clang-format off
  [SYS_fork]    sys_fork,
  [SYS_exit]    sys_exit,
  [SYS_wait]    sys_wait,
  [SYS_pipe]    sys_pipe,
  [SYS_read]    sys_read,
  [SYS_kill]    sys_kill,
  [SYS_exec]    sys_exec,
  [SYS_fstat]   sys_fstat,
  [SYS_chdir]   sys_chdir,
  [SYS_dup]     sys_dup,
  [SYS_getpid]  sys_getpid,
  [SYS_sbrk]    sys_sbrk,
  [SYS_pause]   sys_pause,
  [SYS_uptime]  sys_uptime,
  [SYS_open]    sys_open,
  [SYS_write]   sys_write,
  [SYS_mknod]   sys_mknod,
  [SYS_unlink]  sys_unlink,
  [SYS_link]    sys_link,
  [SYS_mkdir]   sys_mkdir,
  [SYS_close]   sys_close,
  [SYS_ps]      sys_ps,
  [SYS_halt]    sys_halt,
  [SYS_lseek]       sys_lseek,
  [SYS_sem_open]    sys_sem_open,
  [SYS_sem_wait]    sys_sem_wait,
  [SYS_sem_post]    sys_sem_post,
  [SYS_sem_get]     sys_sem_get,
  [SYS_sem_close]   sys_sem_close,
  [SYS_shmget]      sys_shmget,
  [SYS_shmat]       sys_shmat,
  [SYS_shmdt]       sys_shmdt,
  // merge-0617 socket syscalls
  [SYS_socket]       sys_socket,
  [SYS_bind]         sys_bind,
  [SYS_sendto]       sys_sendto,
  [SYS_recvfrom]     sys_recvfrom,
  // tfc extended syscalls (renumbered after socket)
  [SYS_waitpid]             sys_waitpid,
  [SYS_sched_algorithm]     sys_sched_algorithm,
  [SYS_settimeslice]       sys_settimeslice,
  [SYS_gettimeslice]       sys_gettimeslice,
  [SYS_cgettimeofday]      sys_cgettimeofday,
  [SYS_schedstat]          sys_schedstat,
  [SYS_sched_setburst]     sys_sched_setburst,
  [SYS_banker_init]          sys_banker_init,
  [SYS_banker_setmax]        sys_banker_setmax,
  [SYS_banker_request]       sys_banker_request,
  [SYS_banker_release]       sys_banker_release,
  [SYS_banker_safe_sequence] sys_banker_safe_sequence,
  [SYS_banker_get_state]     sys_banker_get_state,
  [SYS_banker_setmax_alloc]  sys_banker_setmax_alloc,
  [SYS_mon_create]           sys_mon_create,
  [SYS_mon_lock]             sys_mon_lock,
  [SYS_mon_unlock]           sys_mon_unlock,
  [SYS_mon_wait]             sys_mon_wait,
  [SYS_mon_signal]           sys_mon_signal,
  [SYS_mon_broadcast]        sys_mon_broadcast,
  [SYS_deadlock_set]         sys_deadlock_set,
  [SYS_setpriority]          sys_setpriority,
  [SYS_getpriority]          sys_getpriority,
  [SYS_rt_register]          sys_rt_register,
  [SYS_rt_wait_period]       sys_rt_wait_period,
  [SYS_getcpuid]             sys_getcpuid,
  [SYS_setcpuaffinity]       sys_setcpuaffinity,
  [SYS_msgget]               sys_msgget,
  [SYS_msgsnd]              sys_msgsnd,
  [SYS_msgrcv]              sys_msgrcv,
  // clang-format on
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
