//
// driver for qemu's virtio disk devices.
// uses qemu's mmio interface to virtio.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "virtio.h"

struct disk {
  uint devno;
  uint64 mmio_base;
  int irq;

  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used *used;

  char free[NUM];
  uint16 used_idx;

  struct {
    struct buf *b;
    char status;
  } info[NUM];

  struct virtio_blk_req ops[NUM];
  struct spinlock vdisk_lock;
};

static struct disk disks[NDISK] = {
  {.devno = ROOTDEV, .mmio_base = VIRTIO0, .irq = VIRTIO0_IRQ},
  {.devno = DISK1DEV, .mmio_base = VIRTIO1, .irq = VIRTIO1_IRQ},
};

static inline volatile uint32 *
R(struct disk *d, int r)
{
  return (volatile uint32 *)(d->mmio_base + r);
}

static struct disk *
disk_for_dev(uint dev)
{
  for (int i = 0; i < NDISK; i++) {
    if (disks[i].devno == dev)
      return &disks[i];
  }
  panic("unknown disk dev");
}

static struct disk *
disk_for_irq(int irq)
{
  for (int i = 0; i < NDISK; i++) {
    if (disks[i].irq == irq)
      return &disks[i];
  }
  panic("unknown disk irq");
}

static int
alloc_desc(struct disk *d)
{
  for (int i = 0; i < NUM; i++) {
    if (d->free[i]) {
      d->free[i] = 0;
      return i;
    }
  }
  return -1;
}

static void
free_desc(struct disk *d, int i)
{
  if (i >= NUM)
    panic("free_desc 1");
  if (d->free[i])
    panic("free_desc 2");
  d->desc[i].addr = 0;
  d->desc[i].len = 0;
  d->desc[i].flags = 0;
  d->desc[i].next = 0;
  d->free[i] = 1;
  wakeup(&d->free[0]);
}

static void
free_chain(struct disk *d, int i)
{
  while (1) {
    int flag = d->desc[i].flags;
    int nxt = d->desc[i].next;
    free_desc(d, i);
    if (flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

static int
alloc3_desc(struct disk *d, int *idx)
{
  for (int i = 0; i < 3; i++) {
    idx[i] = alloc_desc(d);
    if (idx[i] < 0) {
      for (int j = 0; j < i; j++)
        free_desc(d, idx[j]);
      return -1;
    }
  }
  return 0;
}

static void
virtio_disk_init_one(struct disk *d)
{
  uint32 status = 0;

  initlock(&d->vdisk_lock, "virtio_disk");

  if (*R(d, VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
      *R(d, VIRTIO_MMIO_VERSION) != 2 || *R(d, VIRTIO_MMIO_DEVICE_ID) != 2 ||
      *R(d, VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
    panic("could not find virtio disk");
  }

  *R(d, VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(d, VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(d, VIRTIO_MMIO_STATUS) = status;

  uint64 features = *R(d, VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(d, VIRTIO_MMIO_DRIVER_FEATURES) = features;

  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(d, VIRTIO_MMIO_STATUS) = status;

  status = *R(d, VIRTIO_MMIO_STATUS);
  if (!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio disk FEATURES_OK unset");

  *R(d, VIRTIO_MMIO_QUEUE_SEL) = 0;
  if (*R(d, VIRTIO_MMIO_QUEUE_READY))
    panic("virtio disk should not be ready");

  uint32 max = *R(d, VIRTIO_MMIO_QUEUE_NUM_MAX);
  if (max == 0)
    panic("virtio disk has no queue 0");
  if (max < NUM)
    panic("virtio disk max queue too short");

  d->desc = kalloc();
  d->avail = kalloc();
  d->used = kalloc();
  if (!d->desc || !d->avail || !d->used)
    panic("virtio disk kalloc");
  memset(d->desc, 0, PGSIZE);
  memset(d->avail, 0, PGSIZE);
  memset(d->used, 0, PGSIZE);

  *R(d, VIRTIO_MMIO_QUEUE_NUM) = NUM;
  *R(d, VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)d->desc;
  *R(d, VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)d->desc >> 32;
  *R(d, VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)d->avail;
  *R(d, VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)d->avail >> 32;
  *R(d, VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)d->used;
  *R(d, VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)d->used >> 32;

  *R(d, VIRTIO_MMIO_QUEUE_READY) = 0x1;

  for (int i = 0; i < NUM; i++)
    d->free[i] = 1;

  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(d, VIRTIO_MMIO_STATUS) = status;
}

void
virtio_disk_init(void)
{
  for (int i = 0; i < NDISK; i++)
    virtio_disk_init_one(&disks[i]);
}

void
virtio_disk_rw(struct buf *b, int write)
{
  struct disk *d = disk_for_dev(b->dev);
  uint64 sector = b->blockno * (BSIZE / 512);

  acquire(&d->vdisk_lock);

  int idx[3];
  while (1) {
    if (alloc3_desc(d, idx) == 0)
      break;
    sleep(&d->free[0], &d->vdisk_lock);
  }

  struct virtio_blk_req *buf0 = &d->ops[idx[0]];
  if (write)
    buf0->type = VIRTIO_BLK_T_OUT;
  else
    buf0->type = VIRTIO_BLK_T_IN;
  buf0->reserved = 0;
  buf0->sector = sector;

  d->desc[idx[0]].addr = (uint64)buf0;
  d->desc[idx[0]].len = sizeof(struct virtio_blk_req);
  d->desc[idx[0]].flags = VRING_DESC_F_NEXT;
  d->desc[idx[0]].next = idx[1];

  d->desc[idx[1]].addr = (uint64)b->data;
  d->desc[idx[1]].len = BSIZE;
  if (write)
    d->desc[idx[1]].flags = 0;
  else
    d->desc[idx[1]].flags = VRING_DESC_F_WRITE;
  d->desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  d->desc[idx[1]].next = idx[2];

  d->info[idx[0]].status = 0xff;
  d->desc[idx[2]].addr = (uint64)&d->info[idx[0]].status;
  d->desc[idx[2]].len = 1;
  d->desc[idx[2]].flags = VRING_DESC_F_WRITE;
  d->desc[idx[2]].next = 0;

  b->disk = 1;
  d->info[idx[0]].b = b;

  d->avail->ring[d->avail->idx % NUM] = idx[0];
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  d->avail->idx += 1;
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  *R(d, VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

  while (b->disk == 1)
    sleep(b, &d->vdisk_lock);

  d->info[idx[0]].b = 0;
  free_chain(d, idx[0]);

  release(&d->vdisk_lock);
}

void
virtio_disk_intr(int irq)
{
  struct disk *d = disk_for_irq(irq);

  acquire(&d->vdisk_lock);
  *R(d, VIRTIO_MMIO_INTERRUPT_ACK) = *R(d, VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;
  __atomic_thread_fence(__ATOMIC_SEQ_CST);

  while (d->used_idx != d->used->idx) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    int id = d->used->ring[d->used_idx % NUM].id;

    if (d->info[id].status != 0)
      panic("virtio_disk_intr status");

    struct buf *b = d->info[id].b;
    b->disk = 0;
    wakeup(b);
    d->used_idx += 1;
  }

  release(&d->vdisk_lock);
}
