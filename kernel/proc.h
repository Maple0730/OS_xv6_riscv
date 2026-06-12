// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

#define NWCHAN 64

// Shared memory regions
#define NSHM 16  // Maximum number of shared memory regions
#define PGSIZE 4096

// Forward declaration - actual definition is in shm.h

struct waitbucket {
  struct spinlock lock;
  struct proc *head;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;      // The process running on this cpu, or null.
  struct context context; // swtch() here to enter scheduler().
  int noff;               // Depth of push_off() nesting.
  int intena;             // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// prepare_return() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;          // 保护该结构体大多数字段的自旋锁

  // p->lock must be held when using these:
  enum procstate state;          // 进程状态：UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE
  void *chan;                    // 若非零，表示进程正在此 chan 上等待（睡眠）
  int killed;                    // 若非零，表示进程已被杀死，应退出
  int xstate;                    // 退出状态，将返回给父进程的 wait()
  int pid;                       // 进程ID进程标识符
  struct proc *wnext;            // 等待队列中的后继进程
  struct proc *wprev;            // 等待队列中的前驱进程
  struct waitbucket *wbucket;    // 当前挂接的等待桶

  // wait_lock must be held when using this:
  struct proc *parent;           // 父进程指针（需持有 wait_lock 访问）

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;                 // 内核栈的虚拟地址
  uint64 sz;                     // 进程用户内存大小（字节）
  pagetable_t pagetable;         // 用户页表（物理地址，用于 satp）
  struct trapframe *trapframe;   // trap 帧，保存用户态寄存器（位于内核栈顶）
  struct context context;        // 内核线程上下文（用于 swtch() 保存/恢复）

  struct file *ofile[NOFILE];    // 打开的文件描述符表
  struct inode *cwd;             // 当前工作目录的 inode
  char name[16];                 // 进程名称（调试用）

  // 调度相关字段
  uint64 ctime;                 // 进程创建时间（ticks）
  int queue_level;              // MLFQ 队列级别 (0=最高, 1=中, 2=最低)
  int timeslice_used;           // 本时间片已用 tick 数
  uint64 last_sched;           // 上次被调度的时间（ticks）
  int priority;                 // 进程优先级 (0-10, 0最高)
};
