#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"

#define KMEM_NUM_CLASSES 7
#define SLAB_MAGIC 0x51ab51abU

struct kmem_obj {
  struct kmem_obj *next;
};

struct slab_page {
  uint magic;
  ushort class_idx;
  ushort obj_size;
  ushort total_objs;
  ushort free_objs;
};

struct kmem_class {
  struct spinlock lock;
  struct kmem_obj *freelist;
  uint size;
  uint npages;
  uint ntotal;
  uint nfree;
};

static struct kmem_class classes[KMEM_NUM_CLASSES];
static struct {
  uint alloc_ok;
  uint alloc_fail;
  uint free_ok;
} kmem_stats;

static uint class_sizes[KMEM_NUM_CLASSES] = {32, 64, 128, 256, 512, 1024, 2048};
static char *class_names[KMEM_NUM_CLASSES] = {
  "kmalloc32",
  "kmalloc64",
  "kmalloc128",
  "kmalloc256",
  "kmalloc512",
  "kmalloc1024",
  "kmalloc2048",
};

static uint
align_up(uint n, uint align)
{
  return (n + align - 1) & ~(align - 1);
}

static int
class_index(uint n)
{
  for (int i = 0; i < KMEM_NUM_CLASSES; i++) {
    if (n <= classes[i].size)
      return i;
  }
  return -1;
}

static struct slab_page *
slab_page_for(void *p)
{
  return (struct slab_page *)PGROUNDDOWN((uint64)p);
}

static uint
slab_header_size(void)
{
  return align_up(sizeof(struct slab_page), sizeof(uint64));
}

static int
grow_class(struct kmem_class *kc, int idx)
{
  char *page = kalloc();
  if (page == 0)
    return -1;

  memset(page, 0, PGSIZE);

  uint hdrsz = slab_header_size();
  if (hdrsz >= PGSIZE)
    panic("kmalloc: slab header too large");

  uint count = (PGSIZE - hdrsz) / kc->size;
  if (count == 0)
    panic("kmalloc: class too large");

  struct slab_page *sp = (struct slab_page *)page;
  sp->magic = SLAB_MAGIC;
  sp->class_idx = idx;
  sp->obj_size = kc->size;
  sp->total_objs = count;
  sp->free_objs = count;

  char *objbase = page + hdrsz;
  for (uint i = 0; i < count; i++) {
    struct kmem_obj *obj = (struct kmem_obj *)(objbase + i * kc->size);
    obj->next = kc->freelist;
    kc->freelist = obj;
  }

  kc->npages++;
  kc->ntotal += count;
  kc->nfree += count;
  return 0;
}

void
kheapinit(void)
{
  kmem_stats.alloc_ok = 0;
  kmem_stats.alloc_fail = 0;
  kmem_stats.free_ok = 0;
  for (int i = 0; i < KMEM_NUM_CLASSES; i++) {
    initlock(&classes[i].lock, class_names[i]);
    classes[i].freelist = 0;
    classes[i].size = class_sizes[i];
    classes[i].npages = 0;
    classes[i].ntotal = 0;
    classes[i].nfree = 0;
  }
}

void *
kmalloc(uint n)
{
  if (n == 0)
    return 0;

  int idx = class_index(n);
  if (idx < 0) {
    kmem_stats.alloc_fail++;
    return 0;
  }

  struct kmem_class *kc = &classes[idx];
  acquire(&kc->lock);

  if (kc->freelist == 0 && grow_class(kc, idx) < 0) {
    release(&kc->lock);
    kmem_stats.alloc_fail++;
    return 0;
  }

  struct kmem_obj *obj = kc->freelist;
  if (obj == 0)
    panic("kmalloc: empty freelist");

  kc->freelist = obj->next;

  struct slab_page *sp = slab_page_for(obj);
  if (sp->magic != SLAB_MAGIC || sp->class_idx != idx)
    panic("kmalloc: bad slab page");
  if (sp->free_objs == 0 || kc->nfree == 0)
    panic("kmalloc: free count underflow");

  sp->free_objs--;
  kc->nfree--;
  release(&kc->lock);

  kmem_stats.alloc_ok++;
  memset(obj, 0, kc->size);
  return obj;
}

void
kmfree(void *p)
{
  if (p == 0)
    return;

  struct slab_page *sp = slab_page_for(p);
  if (sp->magic != SLAB_MAGIC)
    panic("kmfree: bad slab magic");
  if (sp->class_idx >= KMEM_NUM_CLASSES)
    panic("kmfree: bad slab class");

  struct kmem_class *kc = &classes[sp->class_idx];
  uint hdrsz = slab_header_size();
  uint64 base = (uint64)sp + hdrsz;
  uint64 addr = (uint64)p;
  if (addr < base || addr >= (uint64)sp + PGSIZE)
    panic("kmfree: pointer out of range");
  if (((addr - base) % kc->size) != 0)
    panic("kmfree: pointer misaligned");

  acquire(&kc->lock);
  if (sp->free_objs >= sp->total_objs)
    panic("kmfree: too many free objects");

  memset(p, 1, kc->size);
  struct kmem_obj *obj = (struct kmem_obj *)p;
  obj->next = kc->freelist;
  kc->freelist = obj;
  sp->free_objs++;
  kc->nfree++;
  release(&kc->lock);
  kmem_stats.free_ok++;
}

static void
kmallocdump(void)
{
  uint total_pages = 0;
  uint total_objs = 0;
  uint free_objs = 0;

  printf("kmalloc stats:\n");
  printf("  class  size  pages  total  free  used\n");
  for (int i = 0; i < KMEM_NUM_CLASSES; i++) {
    uint used = classes[i].ntotal - classes[i].nfree;
    printf("  %d %d %d %d %d %d\n",
           i,
           classes[i].size,
           classes[i].npages,
           classes[i].ntotal,
           classes[i].nfree,
           used);
    total_pages += classes[i].npages;
    total_objs += classes[i].ntotal;
    free_objs += classes[i].nfree;
  }
  printf("  alloc_ok=%d alloc_fail=%d free_ok=%d\n",
         kmem_stats.alloc_ok,
         kmem_stats.alloc_fail,
         kmem_stats.free_ok);
  printf("  total_pages=%d total_objs=%d free_objs=%d used_objs=%d\n",
         total_pages,
         total_objs,
         free_objs,
         total_objs - free_objs);
}

void
kmalloctest(void)
{
  printf("kmalloctest: begin\n");
  if (kmalloc(0) != 0)
    panic("kmalloctest: kmalloc(0)");
  if (kmalloc(2049) != 0)
    panic("kmalloctest: kmalloc(2049)");
  printf("kmalloctest: boundary checks ok\n");

  char *a = (char *)kmalloc(1);
  char *b = (char *)kmalloc(32);
  char *c = (char *)kmalloc(33);
  char *d = (char *)kmalloc(2000);
  if (a == 0 || b == 0 || c == 0 || d == 0)
    panic("kmalloctest: basic alloc");
  printf("kmalloctest: basic allocations ok\n");

  a[0] = 'A';
  b[0] = 'B';
  b[31] = 'C';
  c[0] = 'D';
  c[32] = 'E';
  d[0] = 'F';
  d[1999] = 'G';
  if (a[0] != 'A' || b[0] != 'B' || b[31] != 'C' || c[0] != 'D' ||
      c[32] != 'E' || d[0] != 'F' || d[1999] != 'G')
    panic("kmalloctest: writable");
  printf("kmalloctest: read/write checks ok\n");

  kmfree(a);
  char *reuse = (char *)kmalloc(1);
  if (reuse != a)
    panic("kmalloctest: reuse");
  kmfree(reuse);
  printf("kmalloctest: reuse check ok\n");

  enum { CROSS_PAGE_ALLOCS = 140 };
  void *objs[CROSS_PAGE_ALLOCS];
  for (int i = 0; i < CROSS_PAGE_ALLOCS; i++) {
    objs[i] = kmalloc(32);
    if (objs[i] == 0)
      panic("kmalloctest: cross-page alloc");
    ((char *)objs[i])[0] = (char)i;
  }
  for (int i = 0; i < CROSS_PAGE_ALLOCS; i++) {
    if (((char *)objs[i])[0] != (char)i)
      panic("kmalloctest: cross-page write");
  }
  for (int i = 0; i < CROSS_PAGE_ALLOCS; i++)
    kmfree(objs[i]);
  printf("kmalloctest: cross-page growth ok (%d objects)\n", CROSS_PAGE_ALLOCS);

  kmfree(b);
  kmfree(c);
  kmfree(d);

  kmfree(0);
  kmallocdump();
  printf("kmalloctest: ok\n");
}
