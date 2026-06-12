// Network protocol structures

#define ETH_ADDR_LEN   6
#define IP_ADDR_LEN    4

// ── Protocol Type Codes ──
#define ETH_TYPE_IP   0x0800
#define ETH_TYPE_ARP  0x0806

#define IP_PROTO_UDP  17

// ── Ethernet Header ──
struct ethhdr {
  uint8  dst[ETH_ADDR_LEN];
  uint8  src[ETH_ADDR_LEN];
  uint16 type;         // network byte order
} __attribute__((packed));

// ── ARP Header ──
struct arphdr {
  uint16 htype;       // hardware type: 1 = ethernet
  uint16 ptype;       // protocol type: 0x0800 = IPv4
  uint8  hlen;        // hardware addr length: 6
  uint8  plen;        // protocol addr length: 4
  uint16 oper;        // 1 = request, 2 = reply
  uint8  sha[ETH_ADDR_LEN];  // sender hardware addr
  uint8  spa[IP_ADDR_LEN];   // sender protocol addr
  uint8  tha[ETH_ADDR_LEN];  // target hardware addr
  uint8  tpa[IP_ADDR_LEN];   // target protocol addr
} __attribute__((packed));

#define ARP_HTYPE_ETH  1
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

// ── IP Header (no options) ──
struct iphdr {
  uint8  ver_ihl;     // version (4 bits) + IHL (4 bits)
  uint8  tos;         // type of service
  uint16 tot_len;     // total length (network byte order)
  uint16 id;          // identification
  uint16 frag_off;    // fragment offset + flags
  uint8  ttl;         // time to live
  uint8  protocol;    // 17 = UDP
  uint16 check;       // header checksum
  uint8  src[IP_ADDR_LEN];
  uint8  dst[IP_ADDR_LEN];
} __attribute__((packed));

#define IP_VER_IHL    0x45   // version 4, IHL 5 (20 bytes)

// ── UDP Header ──
struct udphdr {
  uint16 sport;       // source port (network byte order)
  uint16 dport;       // destination port
  uint16 len;         // UDP length (header + data)
  uint16 check;       // checksum (0 = disabled)
} __attribute__((packed));

// ── Pseudo Header (for UDP checksum) ──
struct udp_pseudo {
  uint8  src[IP_ADDR_LEN];
  uint8  dst[IP_ADDR_LEN];
  uint8  zero;
  uint8  protocol;
  uint16 udp_len;     // network byte order
} __attribute__((packed));

// ── Network Buffers ──
struct mbuf {
  struct mbuf *next;
  char *head;
  char *data;
  uint len;
};

// ── ARP Table Entry ──
struct arp_entry {
  uint8  ip[IP_ADDR_LEN];
  uint8  mac[ETH_ADDR_LEN];
  int    valid;
};

#define ARP_TABLE_SIZE  16
#define ARP_CACHE_TTL   100   // ticks
