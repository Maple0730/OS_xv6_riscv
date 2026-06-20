#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// ntents oCof the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGBLOCKS];
};

struct log {
  struct spinlock lock;
  int start;
  int dev;
  struct logheader lh;
};
static struct log logtable[NDISK];
static struct {
  struct spinlock lock;
  int outstanding;
  int committing;
  int initialized;
} logstate;

static struct log *log_for_dev(uint dev);
static void recover_from_log(struct log *);
static void commit(struct log *);
static void commit_all(void);

void//初始化
initlog(int dev, struct superblock *sb)
{
  struct log *log = log_for_dev(dev);

  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  if (logstate.initialized == 0) {
    initlock(&logstate.lock, "logstate");
    logstate.initialized = 1;
  }

  initlock(&log->lock, "log");
  log->start = sb->logstart;
  log->dev = dev;
  recover_from_log(log);
}

// Copy committed blocks from log to their home location
static void
install_trans(struct log *log, int recovering)
{
  int tail;

  for (tail = 0; tail < log->lh.n; tail++) {
    if (recovering) {
      printf("recovering tail %d dst %d\n", tail, log->lh.block[tail]);
    }
    struct buf *lbuf = bread(log->dev, log->start + tail + 1); // read log block
    struct buf *dbuf = bread(log->dev, log->lh.block[tail]);   // read dst
    memmove(dbuf->data, lbuf->data, BSIZE); // copy block to dst
    bwrite(dbuf);                           // write dst to disk
    if (recovering == 0)
      bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void//把磁盘中的日志头读到内存中
read_head(struct log *log)
{
  struct buf *buf = bread(log->dev, log->start);
  struct logheader *lh = (struct logheader *)(buf->data);//流式数据转结构数据
  int i;
  log->lh.n = lh->n;
  for (i = 0; i < log->lh.n; i++) {
    log->lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void//把内存中的日志头写到磁盘中
write_head(struct log *log)
{
  struct buf *buf = bread(log->dev, log->start);
  struct logheader *hb = (struct logheader *)(buf->data);
  int i;
  hb->n = log->lh.n;
  for (i = 0; i < log->lh.n; i++) {
    hb->block[i] = log->lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(struct log *log)
{
  read_head(log);
  install_trans(log, 1); // if committed, copy from log to disk
  log->lh.n = 0;
  write_head(log); // clear the log
}

// called at the start of each FS system call.
void//开始操作
begin_op(uint dev)
{
  (void)dev;

  acquire(&logstate.lock);
  while (1) {
    int blocked = logstate.committing;
    for (int i = 0; i < NDISK && !blocked; i++) {
      struct log *log = &logtable[i];
      acquire(&log->lock);
      if (log->lh.n + (logstate.outstanding + 1) * MAXOPBLOCKS > LOGBLOCKS)
        blocked = 1;
      release(&log->lock);
    }
    if (!blocked) {
      logstate.outstanding += 1;
      release(&logstate.lock);
      break;
    }
    sleep(&logstate, &logstate.lock);
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void//结束操作
end_op(uint dev)
{
  (void)dev;
  int do_commit = 0;

  acquire(&logstate.lock);
  logstate.outstanding -= 1;
  if (logstate.committing)
    panic("log.committing");
  if (logstate.outstanding == 0) {
    do_commit = 1;
    logstate.committing = 1;
  } else {
    wakeup(&logstate);
  }
  release(&logstate.lock);

  if (do_commit) {
    commit_all();
    acquire(&logstate.lock);
    logstate.committing = 0;
    wakeup(&logstate);
    release(&logstate.lock);
  }
}

// Copy modified blocks from cache to log.
static void
write_log(struct log *log)
{
  int tail;

  for (tail = 0; tail < log->lh.n; tail++) {
    struct buf *to = bread(log->dev, log->start + tail + 1); // log block
    struct buf *from = bread(log->dev, log->lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to); // write the log
    brelse(from);
    brelse(to);
  }
}

static void
commit(struct log *log)
{
  if (log->lh.n > 0) {
    write_log(log);      // Write modified blocks from cache to log
    write_head(log);     // Write header to disk -- the real commit
    install_trans(log, 0); // Now install writes to home locations
    log->lh.n = 0;
    write_head(log); // Erase the transaction from the log
  }
}

static void
commit_all(void)
{
  for (int i = 0; i < NDISK; i++)
    commit(&logtable[i]);
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  struct log *log = log_for_dev(b->dev);
  int i;

  acquire(&logstate.lock);
  if (logstate.outstanding < 1) {
    release(&logstate.lock);
    panic("log_write outside of trans");
  }
  release(&logstate.lock);

  acquire(&log->lock);
  if (log->lh.n >= LOGBLOCKS)
    panic("too big a transaction");

  for (i = 0; i < log->lh.n; i++) {
    if (log->lh.block[i] == b->blockno) // log absorption
      break;
  }
  log->lh.block[i] = b->blockno;
  if (i == log->lh.n) { // Add new block to log?
    bpin(b);
    log->lh.n++;
  }
  release(&log->lock);
}

static struct log *
log_for_dev(uint dev)
{
  if (dev < ROOTDEV || dev >= ROOTDEV + NDISK)
    panic("log_for_dev");
  return &logtable[dev - ROOTDEV];
}
