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

// Semaphore system calls
#define SYS_sem_open  24
#define SYS_sem_wait  25
#define SYS_sem_post  26
#define SYS_sem_get   27
#define SYS_sem_close 28
#define SYS_shmget    29
#define SYS_shmat     30
#define SYS_shmdt     31
#define SYS_waitpid   33
int sem_open(int value);
int sem_wait(int sem_id);
int sem_post(int sem_id);
int sem_get(int sem_id, int *value);
int sem_close(int sem_id);

// Shared memory system calls
int shmget(int key, uint64 *addr);
int shmat(int key, uint64 *addr);
int shmdt(uint64 addr);

// Scheduling system calls
#define SYS_sched_algorithm 34
int sched_algorithm(int algo);
const char* sched_algorithm_name(int algo);

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
