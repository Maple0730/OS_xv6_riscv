// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.
// 文件系统实现。分为五层：
//   + 块层：原始磁盘块的分配器。
//   + 日志层：支持多步更新的崩溃恢复。
//   + 文件层：inode 分配、读写、元数据管理。
//   + 目录层：具有特殊内容（其他 inode 的列表！）的 inode。
//   + 名字层：类似 /usr/rtm/xv6/fs.c 的路径，提供便捷的命名方式。
//
// 本文件包含底层文件系统操作例程。
// （更上层的）系统调用实现在 sysfile.c 中。
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
struct superblock superblocks[NDISK];

struct mountent {
  char name[DIRSIZ];
  uint parent_dev;
  uint parent_inum;
  uint target_dev;
};

static struct mountent mounts[] = {
  {.name = "disk1", .parent_dev = ROOTDEV, .parent_inum = ROOTINO, .target_dev = DISK1DEV},
};

static struct superblock *sb_for(uint dev);
static struct mountent *mount_by_name(struct inode *dp, const char *name);
static struct mountent *mount_by_target(struct inode *ip);

// Read the super block.
static void//读取超级块
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);// 提取出超级块所在的磁盘块（第1块），并将其内容读入内存缓冲区。
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// Init fs
void//文件系统初始化
fsinit(int dev)
{
  struct superblock *sb = sb_for(dev);

  readsb(dev, sb);
  if (sb->magic != FSMAGIC)
    panic("invalid file system");
  initlog(dev, sb);
  ireclaim(dev);
}

int
fsdevvalid(int dev)
{
  return dev >= ROOTDEV && dev < ROOTDEV + NDISK;
}

int
fsmountpoint(struct inode *dp, const char *name)
{
  return mount_by_name(dp, name) != 0;
}

// Zero a block.
static void//清空块
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
// returns 0 if out of disk space.
static uint//分配空闲磁盘块
balloc(uint dev)
{
  struct superblock *sb = sb_for(dev);
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for (b = 0; b < sb->size; b += BPB) {
    bp = bread(dev, BBLOCK(b, (*sb)));
    for (bi = 0; bi < BPB && b + bi < sb->size; bi++) {
      m = 1 << (bi % 8);
      if ((bp->data[bi / 8] & m) == 0) { // Is block free?
        bp->data[bi / 8] |= m;           // Mark block in use.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  printf("balloc: out of blocks\n");
  return 0;
}

// Free a disk block.
static void//释放磁盘块
bfree(int dev, uint b)
{
  struct superblock *sb = sb_for(dev);
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, (*sb)));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if ((bp->data[bi / 8] & m) == 0)
    panic("freeing free block");
  bp->data[bi / 8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at block
// sb.inodestart. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a table of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The in-memory
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in table: an entry in the inode table
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a table entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   table entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays in the table and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The itable.lock spin-lock protects the allocation of itable
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold itable.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

// Inodes（索引节点）: 一个 inode 描述一个未命名的文件。
//
// 磁盘上的 inode 结构存储元数据：文件类型、大小、
// 指向它的链接数，以及保存文件内容的数据块列表。
//
// 磁盘上的 inode 按顺序存放在从 sb.inodestart 开始的块中。
// 每个 inode 都有一个编号，表示其在磁盘上的位置。
//
// 内核在内存中维护一个“使用中的 inode 表”，
// 为多个进程并发访问 inode 提供同步支持。
// 内存中的 inode 包含一些磁盘上不存储的簿记信息：
// ip->ref 和 ip->valid。
//
// 一个 inode 及其内存表示在能被文件系统其余代码使用之前，
// 需要经历一系列状态。
//
//   * 分配（Allocation）：如果磁盘上的 inode 类型（type）非零，
//     则表示该 inode 已被分配。ialloc() 负责分配，
//     而 iput() 会在引用计数和链接计数都降为零时释放它。
//
//   * 表项引用（Referencing in table）：如果 ip->ref 为零，
//     则表示该 inode 表项是空闲的。否则，ip->ref 跟踪
//     指向该表项的内存指针数量（包括打开的文件和当前目录）。
//     iget() 查找或创建表项并增加其引用计数；
//     iput() 则减少引用计数。
//
//   * 有效（Valid）：只有当 ip->valid 为 1 时，
//     inode 表项中的信息（类型、大小等）才是正确的。
//     ilock() 从磁盘读取 inode 并设置 ip->valid，
//     而 iput() 在 ip->ref 降为零时会清除 ip->valid。
//
//   * 锁定（Locked）：文件系统代码只有在先锁定了 inode 之后，
//     才能检查和修改 inode 中的信息及其内容。
//
// 因此，典型的使用流程如下：
//   ip = iget(dev, inum)   // 获取 inode 表项
//   ilock(ip)              // 锁定 inode
//   ... 检查和修改 ip->xxx ...
//   iunlock(ip)            // 解锁 inode
//   iput(ip)               // 释放 inode 表项引用
//
// ilock() 与 iget() 分离的设计，是为了让系统调用能长期持有
// 对 inode 的引用（例如打开文件时），而只在短期内锁定它
// （例如在 read() 中）。这种分离也有助于避免路径名查找
// 过程中的死锁和竞争。iget() 增加 ip->ref 是为了确保
// inode 保留在表中，且指向它的指针始终有效。
//
// 许多内部文件系统函数期望调用者已经锁定了涉及的 inode；
// 这允许调用者创建多步原子操作。
//
// itable.lock 自旋锁保护 itable 表项的分配。
// 由于 ip->ref 指示表项是否空闲，而 ip->dev 和 ip->inum
// 指示表项持有哪个 inode，因此在访问这些字段时必须持有 itable.lock。
//
// ip->lock 睡眠锁保护除 ref、dev 和 inum 之外的所有 ip 字段。
// 要读写该 inode 的 ip->valid、ip->size、ip->type 等字段，
// 必须持有 ip->lock。
struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} itable;

void//初始化inode表
iinit()
{
  int i = 0;

  initlock(&itable.lock, "itable");
  for (i = 0; i < NINODE; i++) {
    initsleeplock(&itable.inode[i].lock, "inode");
  }
}

static struct inode *iget(uint dev, uint inum);

// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode,
// or NULL if there is no free inode.
struct inode *//在磁盘上分配一个新的 inode
ialloc(uint dev, short type)
{
  struct superblock *sb = sb_for(dev);
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for (inum = 1; inum < sb->ninodes; inum++) {
    bp = bread(dev, IBLOCK(inum, (*sb)));
    dip = (struct dinode *)bp->data + inum % IPB;//锁的颗粒度更细
    if (dip->type == 0) { // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp); // mark it allocated on the disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  printf("ialloc: no inodes\n");
  return 0;
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk.
// Caller must hold ip->lock.
void//把内存 inode 的关键字段刷回磁盘
iupdate(struct inode *ip)
{
  struct superblock *sb = sb_for(ip->dev);
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, (*sb)));
  dip = (struct dinode *)bp->data + ip->inum % IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode *//在内存中查找或分配一个 struct inode 表项
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&itable.lock);

  // Is the inode already in the table?
  empty = 0;
  for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++) {
    if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
      ip->ref++;
      release(&itable.lock);
      return ip;
    }
    if (empty == 0 && ip->ref == 0) // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode entry.
  if (empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&itable.lock);

  return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode *//ref++;引用计数加一
idup(struct inode *ip)
{
  acquire(&itable.lock);
  ip->ref++;
  release(&itable.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void//从磁盘拿到内存并上锁
ilock(struct inode *ip)
{
  struct superblock *sb = sb_for(ip->dev);
  struct buf *bp;
  struct dinode *dip;

  if (ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if (ip->valid == 0) {
    bp = bread(ip->dev, IBLOCK(ip->inum, (*sb)));
    dip = (struct dinode *)bp->data + ip->inum % IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if (ip->type == 0)
      panic("ilock: no type");
  }
}

// Unlock the given inode.
void//释放inode sleep锁
iunlock(struct inode *ip)
{
  if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode table entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void//ref--;如果引用计数降为零且链接数也为零，则释放磁盘上的 inode 及其内容
iput(struct inode *ip)
{
  acquire(&itable.lock);

  if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
    // inode has no links and no other references: truncate and free.

    // ip->ref == 1 means no other process can have ip locked,
    // so this acquiresleep() won't block (or deadlock).
    acquiresleep(&ip->lock);

    release(&itable.lock);

    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;

    releasesleep(&ip->lock);

    acquire(&itable.lock);
  }

  ip->ref--;
  release(&itable.lock);
}

// Common idiom: unlock, then put.
//释放一个 inode 的 sleep 锁，并减少引用计数
void
iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

void//用于启动恢复 orphaned inode。 如果磁盘上有 type != 0 && nlink == 0 的 inode，
//说明上次崩溃前它本该被回收但没做完，这里会触发回收闭环
ireclaim(int dev)
{
  struct superblock *sb = sb_for(dev);
  for (int inum = 1; inum < sb->ninodes; inum++) {
    struct inode *ip = 0;
    struct buf *bp = bread(dev, IBLOCK(inum, (*sb)));
    struct dinode *dip = (struct dinode *)bp->data + inum % IPB;
    if (dip->type != 0 && dip->nlink == 0) { // is an orphaned inode
      printf("ireclaim: orphaned inode %d\n", inum);
      ip = iget(dev, inum);
    }
    brelse(bp);
    if (ip) {
      begin_op(dev);
      ilock(ip);
      iunlock(ip);
      iput(ip);
      end_op(dev);
    }
  }
}

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
// returns 0 if out of disk space.

// Inode 的内容
//
// 与每个 inode 关联的数据（内容）存储在磁盘的块中。
// 前 NDIRECT 个数据块的编号列在 ip->addrs[] 中。
// 接下来的 NINDIRECT 个数据块的编号列在块 ip->addrs[NDIRECT] 中。

// 返回 inode ip 的第 n 个数据块的磁盘块号。
// 如果该块尚不存在，则 bmap 会分配一个。
// 若磁盘空间不足，则返回 0。
static uint
// 输入：

// inode ip
// 文件中的逻辑块号 bn
// 输出：

// 对应的真实磁盘块号
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if (bn < NDIRECT) {
    if ((addr = ip->addrs[bn]) == 0) {
      addr = balloc(ip->dev);
      if (addr == 0)
        return 0;
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  bn -= NDIRECT;

  if (bn < NINDIRECT) {
    // Load indirect block, allocating if necessary.
    if ((addr = ip->addrs[NDIRECT]) == 0) {
      addr = balloc(ip->dev);
      if (addr == 0)
        return 0;
      ip->addrs[NDIRECT] = addr;
    }
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    if ((addr = a[bn]) == 0) {
      addr = balloc(ip->dev);
      if (addr) {
        a[bn] = addr;
        log_write(bp);
      }
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Caller must hold ip->lock.
void//责释放一个 inode 的全部内容块
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for (i = 0; i < NDIRECT; i++) {
    if (ip->addrs[i]) {
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if (ip->addrs[NDIRECT]) {
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint *)bp->data;
    for (j = 0; j < NINDIRECT; j++) {
      if (a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

// Copy stat information from inode.
// Caller must hold ip->lock.
void//复制
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

// Read data from inode.
// Caller must hold ip->lock.
// If user_dst==1, then dst is a user virtual address;
// otherwise, dst is a kernel address.
int//从 inode 内容里读数据，返回实际读取字节数
readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;
  //边界检查
  if (off > ip->size || off + n < off)
    return 0;
  if (off + n > ip->size)
    n = ip->size - off;

  for (tot = 0; tot < n; tot += m, off += m, dst += m) {
    uint addr = bmap(ip, off / BSIZE);
    if (addr == 0)
      break;
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off % BSIZE);//计算本次循环要读的字节数，不能超过一个块剩余的字节数
    //either_copyout() 拷给用户或内核地址
    if (either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
      brelse(bp);
      tot = -1;
      break;
    }
    brelse(bp);
  }
  return tot;
}

// Write data to inode.
// Caller must hold ip->lock.
// If user_src==1, then src is a user virtual address;
// otherwise, src is a kernel address.
// Returns the number of bytes successfully written.
// If the return value is less than the requested n,
// there was an error of some kind.
int//向 inode 内容里写数据，返回实际写入字节数
writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if (off > ip->size || off + n < off)
    return -1;
  if (off + n > MAXFILE * BSIZE)
    return -1;

  for (tot = 0; tot < n; tot += m, off += m, src += m) {
    uint addr = bmap(ip, off / BSIZE);
    if (addr == 0)
      break;
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off % BSIZE);
    if (either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
      brelse(bp);
      break;
    }
    log_write(bp);
    brelse(bp);
  }

  if (off > ip->size)
    ip->size = off;

  // write the i-node back to disk even if the size didn't change
  // because the loop above might have called bmap() and added a new
  // block to ip->addrs[].
  iupdate(ip);

  return tot;
}

// Directories

int//比较两个目录项文件名是否相同
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
//在目录 dp 中查找与 name 匹配的目录项，并返回对应的 inode。
//若找到，*poff 被设置为该目录项在目录文件中的字节偏移。
struct inode *
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if (dp->type != T_DIR)
    panic("dirlookup not DIR");

  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if (de.inum == 0)
      continue;
    if (namecmp(name, de.name) == 0) {
      // entry matches path element
      if (poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
// Returns 0 on success, -1 on failure (e.g. out of disk blocks).
int//往目录里插入新项
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.检查该目录下是否已经有同名项
  if ((ip = dirlookup(dp, name, 0)) != 0) {
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if (de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    return -1;

  return 0;
}

// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char *//从路径字符串中提取第一个路径分量，并返回指向剩余路径的指针,name这一层的目录文件名
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while (*path == '/')
    path++;
  if (*path == 0)
    return 0;
  s = path;
  while (*path != '/' && *path != 0)
    path++;
  len = path - s;
  if (len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while (*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
// namex 是 xv6 文件系统中路径解析的核心函数。它根据给定的路径字符串，逐级查找目录，最终返回对应文件的 inode。它支持两种模式：
// 普通模式（nameiparent == 0）：返回路径本身对应的 inode。
// 父目录模式（nameiparent == 1）：返回路径的父目录的 inode，并将路径的最后一个分量（文件名）复制到 name 缓冲区中。
static struct inode *
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;
  struct mountent *mnt;

  if (*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while ((path = skipelem(path, name)) != 0) {
    ilock(ip);
    if (ip->type != T_DIR) {
      iunlockput(ip);
      return 0;
    }
    if (nameiparent && *path == '\0') {
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if (namecmp(name, "..") == 0 && (mnt = mount_by_target(ip)) != 0) {
      next = iget(mnt->parent_dev, mnt->parent_inum);
    } else if ((mnt = mount_by_name(ip, name)) != 0) {
      next = iget(mnt->target_dev, ROOTINO);
    } else if ((next = dirlookup(ip, name, 0)) == 0) {
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if (nameiparent) {
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode *//返回路径最终对象
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode *//返回父目录和最后一段名字
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}

static struct superblock *
sb_for(uint dev)
{
  if (!fsdevvalid(dev))
    panic("sb_for");
  return &superblocks[dev - ROOTDEV];
}

static struct mountent *
mount_by_name(struct inode *dp, const char *name)
{
  for (int i = 0; i < NELEM(mounts); i++) {
    if (dp->dev == mounts[i].parent_dev &&
        dp->inum == mounts[i].parent_inum &&
        namecmp(mounts[i].name, name) == 0)
      return &mounts[i];
  }
  return 0;
}

static struct mountent *
mount_by_target(struct inode *ip)
{
  for (int i = 0; i < NELEM(mounts); i++) {
    if (ip->dev == mounts[i].target_dev && ip->inum == ROOTINO)
      return &mounts[i];
  }
  return 0;
}
