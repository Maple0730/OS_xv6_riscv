// Phase D2: kernel-level message queues.
//
// Syscalls:
//   int msgget(int key, int size)              -- create/get queue with msg size
//   int msgsnd(int qid, char *buf, int len)    -- send a message (block if full)
//   int msgrcv(int qid, char *buf, int len)    -- receive a message (block if empty)
//
// Each queue holds up to NMSG messages of fixed size `size`.
// The size is fixed at msgget time.  msgsnd truncates the
// caller-supplied buffer to `size`.  msgrcv copies up to
// `size` bytes into the caller's buffer.
//
// The implementation uses a circular buffer of messages, a
// spinlock for the queue itself, and the standard xv6
// sleep/wakeup (chan = &queue's "notfull"/"notempty" field)
// for blocking I/O.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "vm.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define NMSGQ 16
#define NMSG 8          // max messages per queue
#define MAX_MSG_SIZE 128

struct message {
  int len;                          // actual message length
  char data[MAX_MSG_SIZE];
};

struct msgq {
  int key;                          // user-visible key
  int allocated;
  int size;                         // fixed message size
  struct spinlock lock;
  int head, tail, count;
  int notfull;                      // chan for senders
  int notempty;                     // chan for receivers
  struct message msgs[NMSG];
};

static struct msgq msgq_table[NMSGQ];
static struct spinlock msgq_table_lock;

void
msgq_init(void)
{
  initlock(&msgq_table_lock, "msgq_table");
  for (int i = 0; i < NMSGQ; i++) {
    msgq_table[i].allocated = 0;
    initlock(&msgq_table[i].lock, "msgq");
  }
}

static int
msgq_find_by_key(int key)
{
  for (int i = 0; i < NMSGQ; i++) {
    if (msgq_table[i].allocated && msgq_table[i].key == key)
      return i;
  }
  return -1;
}

static int
msgq_find_free(void)
{
  for (int i = 0; i < NMSGQ; i++) {
    if (!msgq_table[i].allocated)
      return i;
  }
  return -1;
}

// msgget(key, size) -- create or get a message queue with the
// given fixed message size.  Returns the qid (>=0) on success,
// -1 on failure.  When getting an existing queue, `size` must
// match.
int
msgget(int key, int size)
{
  if (key <= 0 || size <= 0 || size > MAX_MSG_SIZE)
    return -1;

  acquire(&msgq_table_lock);
  int idx = msgq_find_by_key(key);
  if (idx >= 0) {
    if (msgq_table[idx].size != size) {
      release(&msgq_table_lock);
      return -1;
    }
    release(&msgq_table_lock);
    return idx;
  }
  idx = msgq_find_free();
  if (idx < 0) {
    release(&msgq_table_lock);
    return -1;
  }
  msgq_table[idx].allocated = 1;
  msgq_table[idx].key = key;
  msgq_table[idx].size = size;
  msgq_table[idx].head = 0;
  msgq_table[idx].tail = 0;
  msgq_table[idx].count = 0;
  release(&msgq_table_lock);
  return idx;
}

// msgsnd(qid, buf, len) -- send a message.  Blocks if the
// queue is full.  Copies min(len, size) bytes.
int
msgsnd(int qid, char *buf, int len)
{
  if (qid < 0 || qid >= NMSGQ || !msgq_table[qid].allocated)
    return -1;
  int size = msgq_table[qid].size;
  if (len < 0)
    return -1;
  if (len > size) len = size;

  struct proc *p = myproc();
  acquire(&msgq_table[qid].lock);
  while (msgq_table[qid].count == NMSG) {
    // queue full -- block on notfull
    sleep(&msgq_table[qid].notfull, &msgq_table[qid].lock);
  }
  // Copy message from user.
  int slot = msgq_table[qid].tail;
  if (copyin(p->pagetable, msgq_table[qid].msgs[slot].data, (uint64)buf, len) < 0) {
    release(&msgq_table[qid].lock);
    return -1;
  }
  msgq_table[qid].msgs[slot].len = len;
  msgq_table[qid].tail = (msgq_table[qid].tail + 1) % NMSG;
  msgq_table[qid].count++;
  wakeup(&msgq_table[qid].notempty);
  release(&msgq_table[qid].lock);
  return 0;
}

// msgrcv(qid, buf, len) -- receive a message.  Blocks if the
// queue is empty.  Copies up to min(len, size) bytes.
int
msgrcv(int qid, char *buf, int len)
{
  if (qid < 0 || qid >= NMSGQ || !msgq_table[qid].allocated)
    return -1;
  int size = msgq_table[qid].size;
  if (len < 0)
    return -1;
  if (len > size) len = size;

  struct proc *p = myproc();
  acquire(&msgq_table[qid].lock);
  while (msgq_table[qid].count == 0) {
    sleep(&msgq_table[qid].notempty, &msgq_table[qid].lock);
  }
  int slot = msgq_table[qid].head;
  int mlen = msgq_table[qid].msgs[slot].len;
  int cp = mlen < len ? mlen : len;
  if (copyout(p->pagetable, (uint64)buf, msgq_table[qid].msgs[slot].data, cp) < 0) {
    release(&msgq_table[qid].lock);
    return -1;
  }
  msgq_table[qid].head = (msgq_table[qid].head + 1) % NMSG;
  msgq_table[qid].count--;
  wakeup(&msgq_table[qid].notfull);
  release(&msgq_table[qid].lock);
  return cp;
}
