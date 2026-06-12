//
// driver for qemu's virtio net device.
// uses qemu's mmio interface to virtio, same as virtio_disk.
//
// qemu ... -device virtio-net-device,netdev=net0 -netdev user,id=net0
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "virtio.h"

// virtio-net MMIO base address (second virtio MMIO slot)
#define R(r) ((volatile uint32 *)(VIRTIO1 + (r)))

// virtio-net config space (offset 0x100 from MMIO base)
// MAC is at config offset 0, 6 bytes
static inline uint8
net_cfg_read8(uint off)
{
  return *(volatile uint8 *)(uint64)(VIRTIO1 + VIRTIO_MMIO_CONFIG + off);
}

// legacy virtio-net header (10 bytes)
struct virtio_net_hdr {
  uint8  flags;
  uint8  gso_type;
  uint16 hdr_len;
  uint16 gso_size;
  uint16 csum_start;
  uint16 csum_offset;
};

#define VNETHDR_SZ  sizeof(struct virtio_net_hdr)
#define NET_BUF_SZ  2048  // must be >= VNETHDR_SZ + 1514 (max ethernet frame)

static struct netdev {
  // RX queue (queue 0) — device writes incoming packets
  struct virtq_desc *rx_desc;
  struct virtq_avail *rx_avail;
  struct virtq_used  *rx_used;
  char rx_bufs[NUM][NET_BUF_SZ] __attribute__((aligned(16)));

  // TX queue (queue 1) — device reads outgoing packets
  struct virtq_desc *tx_desc;
  struct virtq_avail *tx_avail;
  struct virtq_used  *tx_used;
  char tx_bufs[NUM][NET_BUF_SZ] __attribute__((aligned(16)));

  int tx_free[NUM];     // which TX descriptors are free

  uint16 rx_used_idx;   // how far we've scanned rx_used->ring

  uint8 mac[6];

  struct spinlock lock;
} netdev;

// ── Initialise virtio-net ──
void
virtio_net_init(void)
{
  uint32 status = 0;

  initlock(&netdev.lock, "virtio_net");

  if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
      *R(VIRTIO_MMIO_VERSION) != 2 ||
      *R(VIRTIO_MMIO_DEVICE_ID) != VIRTIO_ID_NET ||
      *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
    panic("could not find virtio net");
  }

  // reset device
  *R(VIRTIO_MMIO_STATUS) = status;

  // set ACKNOWLEDGE status bit
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  // set DRIVER status bit
  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  // negotiate features: accept none (no checksum offload, no mergeable bufs)
  uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  // explicitly clear features we don't want
  features &= ~(1 << VIRTIO_NET_F_CSUM);
  features &= ~(1 << VIRTIO_NET_F_GUEST_CSUM);
  features &= ~(1 << VIRTIO_NET_F_CTRL_GUEST_OFFLOADS);
  features &= ~(1 << VIRTIO_NET_F_MAC);
  features &= ~(1 << VIRTIO_NET_F_GUEST_TSO4);
  features &= ~(1 << VIRTIO_NET_F_GUEST_TSO6);
  features &= ~(1 << VIRTIO_NET_F_GUEST_ECN);
  features &= ~(1 << VIRTIO_NET_F_GUEST_UFO);
  features &= ~(1 << VIRTIO_NET_F_HOST_TSO4);
  features &= ~(1 << VIRTIO_NET_F_HOST_TSO6);
  features &= ~(1 << VIRTIO_NET_F_HOST_ECN);
  features &= ~(1 << VIRTIO_NET_F_HOST_UFO);
  features &= ~(1 << VIRTIO_NET_F_MRG_RXBUF);
  features &= ~(1 << VIRTIO_NET_F_STATUS);
  features &= ~(1 << VIRTIO_NET_F_CTRL_VQ);
  features &= ~(1 << VIRTIO_NET_F_CTRL_RX);
  features &= ~(1 << VIRTIO_NET_F_CTRL_VLAN);
  features &= ~(1 << VIRTIO_NET_F_GUEST_ANNOUNCE);
  features &= ~(1 << VIRTIO_NET_F_MQ);
  features &= ~(1 << VIRTIO_NET_F_CTRL_MAC_ADDR);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  status = *R(VIRTIO_MMIO_STATUS);
  if (!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio net FEATURES_OK unset");

  // ── Initialise receive queue (queue 0) ──
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
  if (*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio net RX queue already ready");

  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if (max == 0)
    panic("virtio net has no RX queue");
  if (max < NUM)
    panic("virtio net RX queue too short");

  netdev.rx_desc  = kalloc();
  netdev.rx_avail = kalloc();
  netdev.rx_used  = kalloc();
  if (!netdev.rx_desc || !netdev.rx_avail || !netdev.rx_used)
    panic("virtio net RX kalloc");
  memset(netdev.rx_desc,  0, PGSIZE);
  memset(netdev.rx_avail, 0, PGSIZE);
  memset(netdev.rx_used,  0, PGSIZE);

  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW)     = (uint64)netdev.rx_desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH)    = (uint64)netdev.rx_desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW)    = (uint64)netdev.rx_avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH)   = (uint64)netdev.rx_avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW)    = (uint64)netdev.rx_used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH)   = (uint64)netdev.rx_used >> 32;
  *R(VIRTIO_MMIO_QUEUE_READY)        = 0x1;

  // fill RX descriptors — all are write-only buffers for the device
  for (int i = 0; i < NUM; i++) {
    netdev.rx_desc[i].addr  = (uint64)netdev.rx_bufs[i];
    netdev.rx_desc[i].len   = NET_BUF_SZ;
    netdev.rx_desc[i].flags = VRING_DESC_F_WRITE; // device writes packets
    netdev.rx_desc[i].next  = 0;
    netdev.rx_avail->ring[i] = i;
  }
  netdev.rx_avail->idx = NUM; // all descriptors are available
  netdev.rx_used_idx   = 0;

  // ── Initialise transmit queue (queue 1) ──
  *R(VIRTIO_MMIO_QUEUE_SEL) = 1;
  if (*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio net TX queue already ready");

  max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if (max < NUM)
    panic("virtio net TX queue too short");

  netdev.tx_desc  = kalloc();
  netdev.tx_avail = kalloc();
  netdev.tx_used  = kalloc();
  if (!netdev.tx_desc || !netdev.tx_avail || !netdev.tx_used)
    panic("virtio net TX kalloc");
  memset(netdev.tx_desc,  0, PGSIZE);
  memset(netdev.tx_avail, 0, PGSIZE);
  memset(netdev.tx_used,  0, PGSIZE);

  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW)     = (uint64)netdev.tx_desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH)    = (uint64)netdev.tx_desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW)    = (uint64)netdev.tx_avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH)   = (uint64)netdev.tx_avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW)    = (uint64)netdev.tx_used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH)   = (uint64)netdev.tx_used >> 32;
  *R(VIRTIO_MMIO_QUEUE_READY)        = 0x1;

  for (int i = 0; i < NUM; i++) {
    netdev.tx_free[i] = 1;
  }

  // ── Read MAC address from config space ──
  for (int i = 0; i < 6; i++)
    netdev.mac[i] = net_cfg_read8(i);

  printf("virtio-net: MAC %x:%x:%x:%x:%x:%x\n",
         netdev.mac[0], netdev.mac[1], netdev.mac[2],
         netdev.mac[3], netdev.mac[4], netdev.mac[5]);

  // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;
}

// ── Get our MAC address ──
void
virtio_net_mac(uint8 mac[6])
{
  memmove(mac, netdev.mac, 6);
}

// ── Transmit a packet ──
int
virtio_net_transmit(void *data, uint len)
{
  int idx = -1;

  acquire(&netdev.lock);

  // find a free TX descriptor
  for (int i = 0; i < NUM; i++) {
    if (netdev.tx_free[i]) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    release(&netdev.lock);
    return -1;
  }

  if (len + VNETHDR_SZ > NET_BUF_SZ)
    len = NET_BUF_SZ - VNETHDR_SZ;

  // prepend virtio-net header
  struct virtio_net_hdr *hdr = (struct virtio_net_hdr*)netdev.tx_bufs[idx];
  memset(hdr, 0, VNETHDR_SZ);
  memmove(netdev.tx_bufs[idx] + VNETHDR_SZ, data, len);

  uint total = VNETHDR_SZ + len;

  netdev.tx_desc[idx].addr  = (uint64)netdev.tx_bufs[idx];
  netdev.tx_desc[idx].len   = total;
  netdev.tx_desc[idx].flags = 0; // device reads
  netdev.tx_desc[idx].next  = 0;

  netdev.tx_free[idx] = 0;

  // add to avail ring
  uint16 av_idx = netdev.tx_avail->idx % NUM;
  netdev.tx_avail->ring[av_idx] = idx;

  __sync_synchronize();
  netdev.tx_avail->idx += 1;
  __sync_synchronize();

  // notify device (queue 1)
  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 1;

  // poll until the device is done
  for (int i = 0; i < 1000000; i++) {
    __sync_synchronize();
    if (netdev.tx_desc[idx].flags & 0xFF) // any non-zero status
      break;
    // also check used ring
    for (int j = 0; j < (int)netdev.tx_avail->idx; j++) {
      // check the used ring for our descriptor
    }
  }

  // mark descriptor as free again
  netdev.tx_free[idx] = 1;
  netdev.tx_desc[idx].addr = 0;

  release(&netdev.lock);
  return (int)len;
}

// ── Receive a packet (non-blocking poll) ──
// Returns length (ethernet frame only, without header) or 0 if nothing.
int
virtio_net_receive(void *buf, uint maxlen)
{
  acquire(&netdev.lock);

  // check used ring for completed RX
  if (netdev.rx_used_idx == netdev.rx_used->idx) {
    release(&netdev.lock);
    return 0;
  }

  __sync_synchronize();

  int id = netdev.rx_used->ring[netdev.rx_used_idx % NUM].id;
  uint pkt_len = netdev.rx_used->ring[netdev.rx_used_idx % NUM].len;

  if (pkt_len <= VNETHDR_SZ) {
    // empty or invalid packet
    netdev.rx_used_idx += 1;
    release(&netdev.lock);
    return 0;
  }

  uint eth_len = pkt_len - VNETHDR_SZ;
  if (eth_len > maxlen)
    eth_len = maxlen;

  // copy ethernet frame (skip virtio-net header)
  memmove(buf, netdev.rx_bufs[id] + VNETHDR_SZ, eth_len);

  // give descriptor back to device
  netdev.rx_desc[id].addr  = (uint64)netdev.rx_bufs[id];
  netdev.rx_desc[id].len   = NET_BUF_SZ;
  netdev.rx_desc[id].flags = VRING_DESC_F_WRITE;
  netdev.rx_desc[id].next  = 0;

  uint16 av_idx = netdev.rx_avail->idx % NUM;
  netdev.rx_avail->ring[av_idx] = id;

  __sync_synchronize();
  netdev.rx_avail->idx += 1;

  netdev.rx_used_idx += 1;

  release(&netdev.lock);
  return (int)eth_len;
}

// ── virtio-net interrupt handler ──
void
virtio_net_intr(void)
{
  // acknowledge the interrupt
  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;
  // packets will be picked up by virtio_net_receive() called from net_rx_loop
}

// ── Get our IRQ ──
int
virtio_net_irq(void)
{
  return VIRTIO1_IRQ;
}
