// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
//一页空闲物理页
struct run {
  struct run *next;
};
//空闲列表
struct {
  struct spinlock lock;
  struct run *freelist;
  uint refcounts[PHYSTOP / PGSIZE]; // per-page reference counts (for COW)
} kmem;

void//初始化内存分配器
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

// index of a physical page in the refcount table
static inline uint
pa2idx(void *pa)
{
  uint idx = ((uint64)pa - KERNBASE) / PGSIZE;
  return idx;
}

// Get current reference count for a physical page (COW helper).
int
kref_get(void *pa)
{
  if ((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    return -1;
  uint idx = pa2idx(pa);
  acquire(&kmem.lock);
  int n = kmem.refcounts[idx];
  release(&kmem.lock);
  return n;
}

// Increment reference count of a physical page (COW helper).
void
kref_inc(void *pa)
{
  if ((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    panic("kref_inc");
  uint idx = pa2idx(pa);
  acquire(&kmem.lock);
  kmem.refcounts[idx]++;
  release(&kmem.lock);
}

void//批量把一段内存加入空闲链表
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void//归还一页物理内存
kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  uint idx = pa2idx(pa);
  if (kmem.refcounts[idx] > 0)
    kmem.refcounts[idx]--;
  if (kmem.refcounts[idx] > 0) {
    // still shared (COW): don't actually free
    release(&kmem.lock);
    return;
  }
  kmem.refcounts[idx] = 0; // clear for safety

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *//分配一页物理内存
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r) {
    memset((char *)r, 5, PGSIZE); // fill with junk
    acquire(&kmem.lock);
    kmem.refcounts[pa2idx(r)] = 1;
    release(&kmem.lock);
  }
  return (void *)r;
}
