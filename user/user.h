#define SBRK_ERROR ((char *)-1)

struct stat;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int *);
int waitpid(int pid, int *status);
int pipe(int *);
int write(int, const void *, int);
int read(int, void *, int);
int close(int);
int kill(int);
int exec(const char *, char **);
int open(const char *, int);
int mknod(const char *, short, short);
int unlink(const char *);
int fstat(int fd, struct stat *);
int link(const char *, const char *);
int mkdir(const char *);
int chdir(const char *);
int dup(int);
int getpid(void);
char *sys_sbrk(int, int);
int pause(int);
int uptime(void);
int ps(void);
int halt(void);
int lseek(int, int, int);

// Network system calls
int socket(int, int, int);
int bind(int, uint32, uint16);
int sendto(int, char*, int, uint32, uint16);
int recvfrom(int, char*, int, uint32*, uint16*);

// Semaphore system calls
#define SYS_sem_open  24
#define SYS_sem_wait  25
#define SYS_sem_post  26
#define SYS_sem_get   27
#define SYS_sem_close 28
#define SYS_shmget    29
#define SYS_shmat     30
#define SYS_shmdt     31
#define SYS_lseek     32
#define SYS_socket    33   // merge-0617
#define SYS_bind      34   // merge-0617
#define SYS_sendto    35   // merge-0617
#define SYS_recvfrom  36   // merge-0617
#define SYS_waitpid   37
#define SYS_sched_algorithm 38
#define SYS_settimeslice    39
#define SYS_gettimeslice    40
#define SYS_cgettimeofday   41
#define SYS_schedstat       42
#define SYS_sched_setburst  43
#define SYS_banker_init          44
#define SYS_banker_setmax        45
#define SYS_banker_request       46
#define SYS_banker_release       47
#define SYS_banker_safe_sequence 48
#define SYS_banker_get_state     49
#define SYS_banker_setmax_alloc  50
#define SYS_mon_create           51
#define SYS_mon_lock             52
#define SYS_mon_unlock           53
#define SYS_mon_wait             54
#define SYS_mon_signal           55
#define SYS_mon_broadcast        56
#define SYS_deadlock_set         57
#define SYS_setpriority          58
#define SYS_getpriority          59
#define SYS_rt_register          60
#define SYS_rt_wait_period       61
#define SYS_getcpuid             62
#define SYS_setcpuaffinity       63
#define SYS_msgget               64
#define SYS_msgsnd               65
#define SYS_msgrcv               66
int sem_open(int value);
int sem_wait(int sem_id);
int sem_post(int sem_id);
int sem_get(int sem_id, int *value);
int sem_close(int sem_id);

// Shared memory system calls
// shmget: key (>0), size, shmflg (IPC_CREAT=0x200)
// returns segment id, or -1 on failure
int shmget(int key, int size, int shmflg);
// shmat: key (>0), output addr pointer
// returns 0 on success, -1 on failure
int shmat(int key, uint64 *addr);
// shmdt: user virtual address previously returned by shmat
// returns 0 on success, -1 on failure
int shmdt(uint64 addr);

// Scheduling system calls (renumbered after socket syscalls)
int sched_algorithm(int algo);
const char* sched_algorithm_name(int algo);
int settimeslice(int queue, int ticks);
int gettimeslice(int queue);
unsigned long cgettimeofday(void);
int sched_setburst(int pid, int est);

// Banker's algorithm system calls (Phase B3)
#define NRES_B   8
#define NPROC_B  16
int banker_init(int nres, int *avail);
int banker_setmax(int pid, int *max);
int banker_setmax_alloc(int pid, int *max, int *alloc);
int banker_request(int pid, int *req);
int banker_release(int pid, int *rel);
int banker_safe_sequence(int *out_seq);
int banker_get_state(void *out_state);

// Monitor system calls (Phase C1)
int mon_create(void);
int mon_lock(int mid);
int mon_unlock(int mid);
int mon_wait(int mid, int cvid);
int mon_signal(int mid, int cvid);
int mon_broadcast(int mid, int cvid);
int deadlock_set(int on);  // Phase B4: enable/disable detector
int setpriority(int pid, int prio);  // Phase A2: set static priority
int getpriority(int pid);            // Phase A2: get static priority
int rt_register(int period, int cost);  // Phase F1: register RT task
int rt_wait_period(void);              // Phase F1: wait for next period
int getcpuid(void);                    // Phase E1: current CPU id
int setcpuaffinity(int pid, int cpuid); // Phase E1: pin process to a CPU
int msgget(int key, int size);          // Phase D2: get/create message queue
int msgsnd(int qid, char *buf, int len); // Phase D2: send message
int msgrcv(int qid, char *buf, int len); // Phase D2: receive message

// Phase F3 (创新点): Stride/Lottery 公平调度系统调用
int stride_setweight(int pid, int weight);
int stride_getstate(int pid, unsigned long *stride, unsigned long *pass, int *weight);

struct sched_stat {
  int pid;
  int queue_level;
  int sched_count;
  unsigned long wait_time;
  unsigned long run_time;
};
int schedstat(int pid, struct sched_stat *stat);

// ulib.c
int stat(const char *, struct stat *);
char *strcpy(char *, const char *);
void *memmove(void *, const void *, int);
char *strchr(const char *, char c);
int strcmp(const char *, const char *);
char *gets(char *, int max);
uint strlen(const char *);
void *memset(void *, int, uint);
int atoi(const char *);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
char *sbrk(int);
char *sbrklazy(int);

// printf.c
void fprintf(int, const char *, ...) __attribute__((format(printf, 2, 3)));
void printf(const char *, ...) __attribute__((format(printf, 1, 2)));

// umalloc.c
void *malloc(uint);
void free(void *);
