#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"

#define KMEM_NUM_CLASSES 7
#define KMEM_MAGIC 0x51ab51abU
#define KMEM_NO_CLASS 0xffffffffU

enum kmem_page_kind {
  KMEM_PAGE_SLAB = 1,
  KMEM_PAGE_BIG = 2,
};

struct kmem_obj {
  struct kmem_obj *next;
};

struct kmem_page {
  uint magic;
  uint kind;
  uint class_idx;
  uint obj_size;
  uint total_objs;
  uint free_objs;
  struct kmem_obj *free_head;
  struct kmem_page *next;
  struct kmem_page *prev;
};

struct kmem_class {
  struct spinlock lock;
  struct kmem_page *pages;
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
  uint big_alloc_ok;
  uint big_free_ok;
  uint big_active_pages;
  uint slab_pages_returned;
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

static struct kmem_page *
page_for(void *p)
{
  return (struct kmem_page *)PGROUNDDOWN((uint64)p);
}

static uint
page_header_size(void)
{
  return align_up(sizeof(struct kmem_page), sizeof(uint64));
}

static char *
page_obj_base(struct kmem_page *kp)
{
  return (char *)kp + page_header_size();
}

static uint
big_object_capacity(void)
{
  return PGSIZE - page_header_size();
}

static void
class_add_page(struct kmem_class *kc, struct kmem_page *kp)
{
  kp->prev = 0;
  kp->next = kc->pages;
  if (kc->pages)
    kc->pages->prev = kp;
  kc->pages = kp;
}

static void
class_remove_page(struct kmem_class *kc, struct kmem_page *kp)
{
  if (kp->prev)
    kp->prev->next = kp->next;
  else
    kc->pages = kp->next;
  if (kp->next)
    kp->next->prev = kp->prev;
  kp->prev = 0;
  kp->next = 0;
}

static struct kmem_page *
find_page_with_space(struct kmem_class *kc)
{
  for (struct kmem_page *kp = kc->pages; kp; kp = kp->next) {
    if (kp->free_objs > 0)
      return kp;
  }
  return 0;
}

static int
grow_class(struct kmem_class *kc, int idx)
{
  char *page = kalloc();
  if (page == 0)
    return -1;

  memset(page, 0, PGSIZE);

  uint hdrsz = page_header_size();
  if (hdrsz >= PGSIZE)
    panic("kmalloc: header too large");

  uint count = (PGSIZE - hdrsz) / kc->size;
  if (count == 0)
    panic("kmalloc: class too large");

  struct kmem_page *kp = (struct kmem_page *)page;
  kp->magic = KMEM_MAGIC;
  kp->kind = KMEM_PAGE_SLAB;
  kp->class_idx = idx;
  kp->obj_size = kc->size;
  kp->total_objs = count;
  kp->free_objs = count;
  kp->free_head = 0;

  char *objbase = page_obj_base(kp);
  for (uint i = 0; i < count; i++) {
    struct kmem_obj *obj = (struct kmem_obj *)(objbase + i * kc->size);
    obj->next = kp->free_head;
    kp->free_head = obj;
  }

  class_add_page(kc, kp);
  kc->npages++;
  kc->ntotal += count;
  kc->nfree += count;
  return 0;
}

static void *
alloc_big(uint n)
{
  if (n > big_object_capacity()) {
    kmem_stats.alloc_fail++;
    return 0;
  }

  char *page = kalloc();
  if (page == 0) {
    kmem_stats.alloc_fail++;
    return 0;
  }

  memset(page, 0, PGSIZE);
  struct kmem_page *kp = (struct kmem_page *)page;
  kp->magic = KMEM_MAGIC;
  kp->kind = KMEM_PAGE_BIG;
  kp->class_idx = KMEM_NO_CLASS;
  kp->obj_size = n;
  kp->total_objs = 1;
  kp->free_objs = 0;
  kp->free_head = 0;
  kp->next = 0;
  kp->prev = 0;

  kmem_stats.alloc_ok++;
  kmem_stats.big_alloc_ok++;
  kmem_stats.big_active_pages++;
  return page_obj_base(kp);
}

void
kheapinit(void)
{
  memset(&kmem_stats, 0, sizeof(kmem_stats));
  for (int i = 0; i < KMEM_NUM_CLASSES; i++) {
    initlock(&classes[i].lock, class_names[i]);
    classes[i].pages = 0;
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
  if (idx < 0)
    return alloc_big(n);

  struct kmem_class *kc = &classes[idx];
  acquire(&kc->lock);

  struct kmem_page *kp = find_page_with_space(kc);
  if (kp == 0) {
    if (grow_class(kc, idx) < 0) {
      release(&kc->lock);
      kmem_stats.alloc_fail++;
      return 0;
    }
    kp = kc->pages;
  }

  struct kmem_obj *obj = kp->free_head;
  if (obj == 0)
    panic("kmalloc: empty page freelist");
  if (kp->free_objs == 0 || kc->nfree == 0)
    panic("kmalloc: free count underflow");

  kp->free_head = obj->next;
  kp->free_objs--;
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

  struct kmem_page *kp = page_for(p);
  if (kp->magic != KMEM_MAGIC)
    panic("kmfree: bad page magic");

  uint hdrsz = page_header_size();
  uint64 base = (uint64)kp + hdrsz;
  uint64 addr = (uint64)p;
  if (addr < base || addr >= (uint64)kp + PGSIZE)
    panic("kmfree: pointer out of range");

  if (kp->kind == KMEM_PAGE_BIG) {
    if (addr != base)
      panic("kmfree: big pointer misaligned");
    if (kp->free_objs != 0 || kp->total_objs != 1)
      panic("kmfree: bad big page state");
    kmem_stats.free_ok++;
    kmem_stats.big_free_ok++;
    if (kmem_stats.big_active_pages == 0)
      panic("kmfree: big page underflow");
    kmem_stats.big_active_pages--;
    kfree((void *)kp);
    return;
  }

  if (kp->kind != KMEM_PAGE_SLAB)
    panic("kmfree: bad page kind");
  if (kp->class_idx >= KMEM_NUM_CLASSES)
    panic("kmfree: bad slab class");

  struct kmem_class *kc = &classes[kp->class_idx];
  if (((addr - base) % kc->size) != 0)
    panic("kmfree: slab pointer misaligned");

  acquire(&kc->lock);
  if (kp->free_objs >= kp->total_objs)
    panic("kmfree: too many free objects");

  memset(p, 1, kc->size);
  struct kmem_obj *obj = (struct kmem_obj *)p;
  obj->next = kp->free_head;
  kp->free_head = obj;
  kp->free_objs++;
  kc->nfree++;
  kmem_stats.free_ok++;

  if (kp->free_objs == kp->total_objs) {
    class_remove_page(kc, kp);
    if (kc->npages == 0 || kc->ntotal < kp->total_objs || kc->nfree < kp->total_objs)
      panic("kmfree: class stats underflow");
    kc->npages--;
    kc->ntotal -= kp->total_objs;
    kc->nfree -= kp->total_objs;
    kmem_stats.slab_pages_returned++;
    release(&kc->lock);
    kfree((void *)kp);
    return;
  }

  release(&kc->lock);
}

static void
kmallocdump(void)
{
  uint slab_pages = 0;
  uint slab_objs = 0;
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
    slab_pages += classes[i].npages;
    slab_objs += classes[i].ntotal;
    free_objs += classes[i].nfree;
  }
  printf("  alloc_ok=%d alloc_fail=%d free_ok=%d\n",
         kmem_stats.alloc_ok,
         kmem_stats.alloc_fail,
         kmem_stats.free_ok);
  printf("  big_alloc_ok=%d big_free_ok=%d big_active_pages=%d\n",
         kmem_stats.big_alloc_ok,
         kmem_stats.big_free_ok,
         kmem_stats.big_active_pages);
  printf("  slab_pages_returned=%d\n", kmem_stats.slab_pages_returned);
  printf("  slab_pages=%d slab_objs=%d free_objs=%d used_objs=%d\n",
         slab_pages,
         slab_objs,
         free_objs,
         slab_objs - free_objs);
}

void
kmalloctest(void)
{
  printf("kmalloctest: begin\n");
  if (kmalloc(0) != 0)
    panic("kmalloctest: kmalloc(0)");

  uint bigmax = big_object_capacity();
  if (kmalloc(bigmax + 1) != 0)
    panic("kmalloctest: oversized alloc");
  printf("kmalloctest: boundary checks ok\n");

  char *a = (char *)kmalloc(1);
  char *b = (char *)kmalloc(32);
  char *c = (char *)kmalloc(33);
  char *d = (char *)kmalloc(2000);
  char *e = (char *)kmalloc(2049);
  char *f = (char *)kmalloc(3000);
  char *g = (char *)kmalloc(bigmax);
  if (a == 0 || b == 0 || c == 0 || d == 0 || e == 0 || f == 0 || g == 0)
    panic("kmalloctest: basic alloc");
  printf("kmalloctest: basic allocations ok\n");

  a[0] = 'A';
  b[0] = 'B';
  b[31] = 'C';
  c[0] = 'D';
  c[32] = 'E';
  d[0] = 'F';
  d[1999] = 'G';
  e[0] = 'H';
  e[2048] = 'I';
  f[0] = 'J';
  f[2999] = 'K';
  g[0] = 'L';
  g[bigmax - 1] = 'M';
  if (a[0] != 'A' || b[0] != 'B' || b[31] != 'C' || c[0] != 'D' ||
      c[32] != 'E' || d[0] != 'F' || d[1999] != 'G' || e[0] != 'H' ||
      e[2048] != 'I' || f[0] != 'J' || f[2999] != 'K' || g[0] != 'L' ||
      g[bigmax - 1] != 'M')
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
  if (classes[0].npages != 1)
    panic("kmalloctest: expected one 32-byte slab page remaining");
  printf("kmalloctest: cross-page growth ok (%d objects)\n", CROSS_PAGE_ALLOCS);

  kmfree(b);
  kmfree(c);
  kmfree(d);
  kmfree(e);
  kmfree(f);
  kmfree(g);

  if (classes[0].npages != 0 || classes[1].npages != 0 || classes[6].npages != 0)
    panic("kmalloctest: slab pages not returned");
  if (kmem_stats.big_active_pages != 0)
    panic("kmalloctest: big pages not returned");

  kmfree(0);
  kmallocdump();
  printf("kmalloctest: ok\n");
}
