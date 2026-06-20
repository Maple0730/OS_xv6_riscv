// clang-format off
struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct shm;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;

#include "shm.h"

// bio.c
void            binit(void);
struct buf*     bread(uint, uint);
void            brelse(struct buf*);
void            bwrite(struct buf*);
void            bpin(struct buf*);
void            bunpin(struct buf*);

// console.c
void            consoleinit(void);
void            consoleintr(int);
void            consputc(int);

// exec.c
int             kexec(char*, char**);

// file.c
struct file*    filealloc(void);
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit(void);
int             fileread(struct file*, uint64, int n);
int             filestat(struct file*, uint64 addr);
int             filewrite(struct file*, uint64, int n);
int             fileseek(struct file*, int, int);
// fs.c
void            fsinit(int);
int             fsdevvalid(int);
int             fsmountpoint(struct inode*, const char*);
int             dirlink(struct inode*, char*, uint);
struct inode*   dirlookup(struct inode*, char*, uint*);
struct inode*   ialloc(uint, short);
struct inode*   idup(struct inode*);
void            iinit();
void            ilock(struct inode*);
void            iput(struct inode*);
void            iunlock(struct inode*);
void            iunlockput(struct inode*);
void            iupdate(struct inode*);
int             namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);
int             readi(struct inode*, int, uint64, uint, uint);
void            stati(struct inode*, struct stat*);
int             writei(struct inode*, int, uint64, uint, uint);
void            itrunc(struct inode*);
void            ireclaim(int);

// kalloc.c
void*           kalloc(void);
void            kfree(void *);
void            kinit(void);
void            memdetect(void);
void            kheapinit(void);
void*           kmalloc(uint);
void            kmfree(void *);
void            kmalloctest(void);
extern uint64   boot_dtb;
extern uint64   phys_ram_start;
extern uint64   phys_ram_end;
extern int      phys_ram_detected;

// log.c
void            initlog(int, struct superblock*);
void            log_write(struct buf*);
void            begin_op(uint);
void            end_op(uint);

// pipe.c
int             pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, uint64, int);
int             pipewrite(struct pipe*, uint64, int);

// printf.c
int             printf(char*, ...) __attribute__ ((format (printf, 1, 2)));
void            panic(char*) __attribute__((noreturn));
void            printfinit(void);

// proc.c
int             cpuid(void);
int             cgettimeofday(void);
int             schedstat(int, uint64);
void            kexit(int);
int             kfork(void);
int             growproc(int);
void            proc_mapstacks(pagetable_t);
pagetable_t     proc_pagetable(struct proc *);
void            proc_freepagetable(pagetable_t, uint64);
int             kkill(int);
int             killed(struct proc*);
void            setkilled(struct proc*);
struct cpu*     mycpu(void);
struct proc*    myproc();
void            procinit(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             kwait(uint64);
int             kwaitpid(int pid, uint64 addr);
void            wakeup(void*);
void            yield(void);
void            rr_scheduler(void) __attribute__((noreturn));
void            fcfs_scheduler(void) __attribute__((noreturn));
void            mlfq_scheduler(void) __attribute__((noreturn));
void            sjf_scheduler(void) __attribute__((noreturn));
void            prio_scheduler(void) __attribute__((noreturn));
void            edf_scheduler(void) __attribute__((noreturn));
int             get_timeslice(int queue_level);
int             ksched_setburst(int pid, uint64 est);
int             ksched_setprio(int pid, int prio);
int             get_rr_fcfs_timeslice(void);
int             get_sched_algorithm(void);
void            mlfq_enqueue(struct proc *p);
int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void            procdump(void);

// swtch.S
void            swtch(struct context*, struct context*);

// spinlock.c
void            acquire(struct spinlock*);
int             holding(struct spinlock*);
void            initlock(struct spinlock*, char*);
void            release(struct spinlock*);
void            push_off(void);
void            pop_off(void);

// sleeplock.c
void            acquiresleep(struct sleeplock*);
void            releasesleep(struct sleeplock*);
int             holdingsleep(struct sleeplock*);
void            initsleeplock(struct sleeplock*, char*);

// sem.c
void            seminit(void);
int             sem_init(int, int);
int             sem_open(int*, int);
int             sem_wait(int);
int             sem_post(int);
int             sem_get(int, int*);
int             sem_close(int);
void            sem_broadcast(int);

// banker.c (Phase B3)
void            banker_init_lock(void);
int             banker_init(int, int*);
int             banker_setmax(int, int*);
int             banker_setmax_alloc(int, int*, int*);
int             banker_request(int, int*);
int             banker_release(int, int*);
int             banker_safe_sequence(int*);
int             banker_get_state(uint64);

// deadlock_detect.c (Phase B4)
void            deadlock_init(void);
void            deadlock_scan(void);
int             deadlock_set_enabled(int);

// msgq.c (Phase D2)
void            msgq_init(void);
int             msgget(int, int);
int             msgsnd(int, char*, int);
int             msgrcv(int, char*, int);

// monitor.c (Phase C1)
int             monitor_init(void);
int             monitor_create(void);
int             monitor_lock(int);
int             monitor_unlock(int);
int             monitor_wait(int, int);
int             monitor_signal(int, int);
int             monitor_broadcast(int, int);

// shm.c
void            shminit(void);
int             shmget(int, uint64, int);
int             shmat(int, uint64 *);
int             shmdt(uint64);
int             is_shm_pa(uint64);

// string.c
int             memcmp(const void*, const void*, uint);
void*           memmove(void*, const void*, uint);
void*           memset(void*, int, uint);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, uint);
char*           strncpy(char*, const char*, int);

// syscall.c
void            argint(int, int*);
int             argstr(int, char*, int);
void            argaddr(int, uint64 *);
int             fetchstr(uint64, char*, int);
int             fetchaddr(uint64, uint64*);
void            syscall();

// trap.c
extern uint     ticks;
void            trapinit(void);
void            trapinithart(void);
extern struct spinlock tickslock;
void            prepare_return(void);

// uart.c
void            uartinit(void);
void            uartintr(void);
void            uartwrite(char [], int);
void            uartputc_sync(int);

// vm.c
extern pagetable_t kernel_pagetable;
void            kvminit(void);
void            kvminithart(void);
void            kvmmap(pagetable_t, uint64, uint64, uint64, int);
int             mappages(pagetable_t, uint64, uint64, uint64, int);
pagetable_t     uvmcreate(void);
uint64          uvmalloc(pagetable_t, uint64, uint64, int);
uint64          uvmdealloc(pagetable_t, uint64, uint64);
int             uvmcopy(pagetable_t, pagetable_t, uint64);
void            uvmfree(pagetable_t, uint64);
void            uvmunmap(pagetable_t, uint64, uint64, int);
void            uvmclear(pagetable_t, uint64);
pte_t *         walk(pagetable_t, uint64, int);
uint64          walkaddr(pagetable_t, uint64);
int             copyout(pagetable_t, uint64, char *, uint64);
int             copyin(pagetable_t, char *, uint64, uint64);
int             copyinstr(pagetable_t, char *, uint64, uint64);
int             ismapped(pagetable_t, uint64);
uint64          vmfault(pagetable_t, uint64, int);

// plic.c
void            plicinit(void);
void            plicinithart(void);
int             plic_claim(void);
void            plic_complete(int);

// virtio_disk.c
void            virtio_disk_init(void);
void            virtio_disk_rw(struct buf *, int);
void            virtio_disk_intr(int);

// virtio_net.c
void            virtio_net_init(void);
void            virtio_net_intr(void);
int             virtio_net_transmit(void *, uint);
int             virtio_net_receive(void *, uint);
void            virtio_net_mac(uint8 mac[6]);
int             virtio_net_irq(void);

// net.c
void            net_init(void);
void            net_rx_loop(void);
int             udp_send(uint32, uint16, uint16, char*, uint);
int             udp_recv(uint32*, uint16*, uint16*, char*, uint);

// sysnet.c
void            sock_close(int);
uint64          sys_socket(void);
uint64          sys_bind(void);
uint64          sys_sendto(void);
uint64          sys_recvfrom(void);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
