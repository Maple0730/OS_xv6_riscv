// Network Protocol Stack: Ethernet, ARP, IP, UDP

#include "types.h"
#include "param.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "net.h"

// ── Our IP address and netmask ──
// Default: 10.0.2.15 (QEMU user-mode networking default guest IP)
static uint8 our_ip[IP_ADDR_LEN]    = {10, 0, 2, 15};
static uint8 gateway_ip[IP_ADDR_LEN] = {10, 0, 2, 2};

// ── ARP Table ──
static struct arp_entry arp_table[ARP_TABLE_SIZE];
static struct spinlock arp_lock;

// ── Utility: network-to-host short ──
static inline uint16
ntohs(uint16 n)
{
  return ((n & 0xFF) << 8) | ((n >> 8) & 0xFF);
}

// ── Utility: host-to-network short ──
static inline uint16
htons(uint16 h)
{
  return ntohs(h);
}

// ── Utility: checksum ──
static uint16
checksum(void *data, uint len)
{
  uint32 sum = 0;
  uint16 *p = (uint16*)data;

  for (uint i = 0; i < len / 2; i++)
    sum += ntohs(p[i]);

  if (len & 1)
    sum += ((uint8*)data)[len - 1] << 8;

  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);

  return htons(~sum & 0xFFFF);
}

// ── ARP: look up / add entry ──
static int
arp_lookup(uint8 ip[IP_ADDR_LEN], uint8 mac[ETH_ADDR_LEN])
{
  acquire(&arp_lock);
  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    if (arp_table[i].valid &&
        memcmp(arp_table[i].ip, ip, IP_ADDR_LEN) == 0) {
      memmove(mac, arp_table[i].mac, ETH_ADDR_LEN);
      release(&arp_lock);
      return 0;
    }
  }
  release(&arp_lock);
  return -1;
}

static void
arp_add(uint8 ip[IP_ADDR_LEN], uint8 mac[ETH_ADDR_LEN])
{
  acquire(&arp_lock);
  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    if (!arp_table[i].valid) {
      memmove(arp_table[i].ip, ip, IP_ADDR_LEN);
      memmove(arp_table[i].mac, mac, ETH_ADDR_LEN);
      arp_table[i].valid = 1;
      break;
    }
  }
  release(&arp_lock);
}

// ── ARP: send request ──
static void
arp_send_request(uint8 target_ip[IP_ADDR_LEN])
{
  uint8 our_mac[ETH_ADDR_LEN];
  virtio_net_mac(our_mac);

  struct {
    struct ethhdr eth;
    struct arphdr arp;
  } __attribute__((packed)) pkt;

  // Ethernet header: broadcast
  memset(pkt.eth.dst, 0xFF, ETH_ADDR_LEN);
  memmove(pkt.eth.src, our_mac, ETH_ADDR_LEN);
  pkt.eth.type = htons(ETH_TYPE_ARP);

  // ARP request
  pkt.arp.htype = htons(ARP_HTYPE_ETH);
  pkt.arp.ptype = htons(ETH_TYPE_IP);
  pkt.arp.hlen  = ETH_ADDR_LEN;
  pkt.arp.plen  = IP_ADDR_LEN;
  pkt.arp.oper  = htons(ARP_OP_REQUEST);
  memmove(pkt.arp.sha, our_mac, ETH_ADDR_LEN);
  memmove(pkt.arp.spa, our_ip, IP_ADDR_LEN);
  memset(pkt.arp.tha, 0, ETH_ADDR_LEN);
  memmove(pkt.arp.tpa, target_ip, IP_ADDR_LEN);

  int ret = virtio_net_transmit(&pkt, sizeof(pkt));
  printf("net: ARP request sent for %d.%d.%d.%d, ret=%d\n",
         target_ip[0], target_ip[1], target_ip[2], target_ip[3], ret);
}

// ── ARP: handle incoming ARP packet ──
static void
arp_recv(struct arphdr *arp, uint len)
{
  if (len < sizeof(struct arphdr))
    return;

  if (ntohs(arp->htype) != ARP_HTYPE_ETH ||
      ntohs(arp->ptype) != ETH_TYPE_IP ||
      arp->hlen != ETH_ADDR_LEN ||
      arp->plen != IP_ADDR_LEN)
    return;

  // Learn sender's mapping
  printf("net: ARP recv from IP %d.%d.%d.%d MAC %x:%x:%x:%x:%x:%x\n",
         arp->spa[0], arp->spa[1], arp->spa[2], arp->spa[3],
         arp->sha[0], arp->sha[1], arp->sha[2],
         arp->sha[3], arp->sha[4], arp->sha[5]);
  arp_add(arp->spa, arp->sha);

  // Is this a request for us?
  if (ntohs(arp->oper) == ARP_OP_REQUEST &&
      memcmp(arp->tpa, our_ip, IP_ADDR_LEN) == 0) {

    uint8 our_mac[ETH_ADDR_LEN];
    virtio_net_mac(our_mac);

    struct {
      struct ethhdr eth;
      struct arphdr arp;
    } __attribute__((packed)) reply;

    memmove(reply.eth.dst, arp->sha, ETH_ADDR_LEN);
    memmove(reply.eth.src, our_mac, ETH_ADDR_LEN);
    reply.eth.type = htons(ETH_TYPE_ARP);

    reply.arp.htype = htons(ARP_HTYPE_ETH);
    reply.arp.ptype = htons(ETH_TYPE_IP);
    reply.arp.hlen  = ETH_ADDR_LEN;
    reply.arp.plen  = IP_ADDR_LEN;
    reply.arp.oper  = htons(ARP_OP_REPLY);
    memmove(reply.arp.sha, our_mac, ETH_ADDR_LEN);
    memmove(reply.arp.spa, our_ip, IP_ADDR_LEN);
    memmove(reply.arp.tha, arp->sha, ETH_ADDR_LEN);
    memmove(reply.arp.tpa, arp->spa, IP_ADDR_LEN);

    virtio_net_transmit(&reply, sizeof(reply));
  }
}

// ── UDP: send datagram ──
// Returns bytes sent (including headers) or -1 on error.
int
udp_send(uint32 dst_ip, uint16 dport,
         uint16 sport, char *data, uint dlen)
{
  uint8 our_mac[ETH_ADDR_LEN];
  uint8 dst_mac[ETH_ADDR_LEN];
  uint8 dst_ip_bytes[IP_ADDR_LEN];
  uint8 src_ip_bytes[IP_ADDR_LEN];

  virtio_net_mac(our_mac);

  // Convert IP from uint32 to byte array
  dst_ip_bytes[0] = (dst_ip >> 24) & 0xFF;
  dst_ip_bytes[1] = (dst_ip >> 16) & 0xFF;
  dst_ip_bytes[2] = (dst_ip >> 8) & 0xFF;
  dst_ip_bytes[3] = dst_ip & 0xFF;

  memmove(src_ip_bytes, our_ip, IP_ADDR_LEN);

  // Resolve destination MAC via ARP
  if (arp_lookup(dst_ip_bytes, dst_mac) < 0) {
    // Send ARP request and wait for reply
    printf("net: ARP who-has %d.%d.%d.%d\n",
           dst_ip_bytes[0], dst_ip_bytes[1],
           dst_ip_bytes[2], dst_ip_bytes[3]);
    arp_send_request(dst_ip_bytes);
    // Simple polling wait
    for (int tries = 0; tries < 2000; tries++) {
      for (volatile int j = 0; j < 5000; j++)
        ;
      net_rx_loop();
      if (arp_lookup(dst_ip_bytes, dst_mac) == 0) {
        printf("net: ARP resolved after %d tries\n", tries);
        break;
      }
    }
    if (arp_lookup(dst_ip_bytes, dst_mac) < 0) {
      printf("net: ARP timeout\n");
      return -1;  // ARP resolution failed
    }
  }

  // Build packet (static to avoid kernel stack overflow — 4KB stack!)
  static struct pkt {
    struct ethhdr eth;
    struct iphdr  ip;
    struct udphdr udp;
    char payload[1500];
  } __attribute__((packed)) pkt;

  uint udp_len = sizeof(struct udphdr) + dlen;
  uint ip_len  = sizeof(struct iphdr) + udp_len;

  // Ethernet
  memmove(pkt.eth.dst, dst_mac, ETH_ADDR_LEN);
  memmove(pkt.eth.src, our_mac, ETH_ADDR_LEN);
  pkt.eth.type = htons(ETH_TYPE_IP);

  // IP
  pkt.ip.ver_ihl = IP_VER_IHL;
  pkt.ip.tos     = 0;
  pkt.ip.tot_len = htons(ip_len);
  pkt.ip.id      = 0;
  pkt.ip.frag_off = 0;
  pkt.ip.ttl     = 64;
  pkt.ip.protocol = IP_PROTO_UDP;
  pkt.ip.check   = 0;
  memmove(pkt.ip.src, src_ip_bytes, IP_ADDR_LEN);
  memmove(pkt.ip.dst, dst_ip_bytes, IP_ADDR_LEN);
  pkt.ip.check = checksum(&pkt.ip, sizeof(struct iphdr));

  // UDP
  pkt.udp.sport  = htons(sport);
  pkt.udp.dport  = htons(dport);
  pkt.udp.len    = htons(udp_len);
  pkt.udp.check  = 0;  // skip UDP checksum for simplicity

  memmove(pkt.payload, data, dlen);

  uint total = sizeof(struct ethhdr) + ip_len;
  int ret = virtio_net_transmit(&pkt, total);
  return (ret < 0) ? -1 : (int)dlen;
}

// ── UDP: receive datagram ──
// Returns payload length, or 0 if nothing available.
// Sets src_ip, src_port from the packet.
int
udp_recv(uint32 *src_ip, uint16 *src_port,
         uint16 *dst_port, char *buf, uint maxlen)
{
  static char raw[2048];
  int len = virtio_net_receive(raw, sizeof(raw));
  if (len <= 0)
    return 0;

  if (len < (int)(sizeof(struct ethhdr) + sizeof(struct iphdr) +
                  sizeof(struct udphdr)))
    return 0;

  struct ethhdr *eth = (struct ethhdr*)raw;

  // Only process IP packets
  if (ntohs(eth->type) != ETH_TYPE_IP) {
    if (ntohs(eth->type) == ETH_TYPE_ARP) {
      arp_recv((struct arphdr*)(raw + sizeof(struct ethhdr)),
               len - sizeof(struct ethhdr));
    }
    return 0;
  }

  struct iphdr *ip = (struct iphdr*)(raw + sizeof(struct ethhdr));

  // Only process UDP packets addressed to us
  if (ip->protocol != IP_PROTO_UDP)
    return 0;

  if (memcmp(ip->dst, our_ip, IP_ADDR_LEN) != 0)
    return 0;  // not for us (we don't do promiscuous)

  uint ip_hdr_len = (ip->ver_ihl & 0xF) * 4;
  struct udphdr *udp = (struct udphdr*)((char*)ip + ip_hdr_len);

  uint udp_len = ntohs(udp->len);
  uint payload_len = udp_len - sizeof(struct udphdr);
  if (payload_len > maxlen)
    payload_len = maxlen;

  // Extract source info
  *src_ip   = ((uint32)ip->src[0] << 24) |
              ((uint32)ip->src[1] << 16) |
              ((uint32)ip->src[2] << 8)  |
              ((uint32)ip->src[3]);
  *src_port = ntohs(udp->sport);
  *dst_port = ntohs(udp->dport);

  memmove(buf, (char*)udp + sizeof(struct udphdr), payload_len);

  return (int)payload_len;
}

// ── Process any pending received packets ──
// Called periodically to handle ARP replies etc.
void
net_rx_loop(void)
{
  static char buf[2048];
  uint32 sip;
  uint16 sport, dport;

  // Just poll until the receive ring is empty
  for (int i = 0; i < 8; i++) {
    int r = udp_recv(&sip, &sport, &dport, buf, sizeof(buf));
    if (r <= 0)
      break;
    // UDP packets received here are not delivered to any socket;
    // they are just processed for ARP replies. Actual delivery
    // happens when the application calls recvfrom().
  }
}

// ── Initialise network stack ──
void
net_init(void)
{
  initlock(&arp_lock, "arp");

  // Clear ARP table
  for (int i = 0; i < ARP_TABLE_SIZE; i++)
    arp_table[i].valid = 0;

  // Add gateway to ARP table (we'll resolve it on first use)
  printf("net: IP %d.%d.%d.%d  GW %d.%d.%d.%d\n",
         our_ip[0], our_ip[1], our_ip[2], our_ip[3],
         gateway_ip[0], gateway_ip[1], gateway_ip[2], gateway_ip[3]);
}
