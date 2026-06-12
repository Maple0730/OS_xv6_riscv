// Network system calls: socket, bind, sendto, recvfrom

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "defs.h"
#include "fs.h"
#include "net.h"
#include "proc.h"
#include "file.h"

// ── Socket structure ──
// We support a small fixed number of UDP sockets.
#define NSOCK 16

struct socket {
  int   used;
  int   fd;         // file descriptor number
  uint32 raddr;     // remote IP (network byte order)
  uint16 lport;     // local port (host byte order)
  uint16 rport;     // remote port (host byte order)
  // receive ring buffer (circular)
  char  rbuf[4096];
  int   rhead;      // next byte to read
  int   rtail;      // next byte to write
  int   rcount;     // bytes available
  struct spinlock lock;
};

static struct socket socktab[NSOCK];
static struct spinlock socktab_lock;
static int socktab_initialized = 0;

static void
socktab_init(void)
{
  initlock(&socktab_lock, "socktab");
  for (int i = 0; i < NSOCK; i++) {
    initlock(&socktab[i].lock, "sock");
    socktab[i].used = 0;
  }
  socktab_initialized = 1;
}

// ── sys_socket: create a UDP socket ──
//   int socket(int domain, int type, int protocol)
//   Returns socket fd on success, -1 on failure.
uint64
sys_socket(void)
{
  int domain, type, protocol;
  struct file *f;

  if (!socktab_initialized)
    socktab_init();

  argint(0, &domain);
  argint(1, &type);
  argint(2, &protocol);

  // We only support AF_INET (2) + SOCK_DGRAM (2) + UDP (17)
  if (domain != 2 || type != 2 || protocol != 17)
    return -1;

  acquire(&socktab_lock);
  int idx = -1;
  for (int i = 0; i < NSOCK; i++) {
    if (!socktab[i].used) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    release(&socktab_lock);
    return -1;
  }

  socktab[idx].used   = 1;
  socktab[idx].lport  = 0;
  socktab[idx].rport  = 0;
  socktab[idx].raddr  = 0;
  socktab[idx].rhead  = 0;
  socktab[idx].rtail  = 0;
  socktab[idx].rcount = 0;
  release(&socktab_lock);

  // Allocate a file descriptor for this socket
  f = filealloc();
  if (f == 0) {
    socktab[idx].used = 0;
    return -1;
  }
  f->type     = FD_SOCK;
  f->readable = 1;
  f->writable = 1;
  f->sockid   = idx;

  // Assign fd
  struct proc *p = myproc();
  for (int fd = 0; fd < NOFILE; fd++) {
    if (p->ofile[fd] == 0) {
      p->ofile[fd] = f;
      socktab[idx].fd = fd;
      return fd;
    }
  }

  // No free fd slot
  fileclose(f);
  socktab[idx].used = 0;
  return -1;
}

// ── sys_bind: bind a socket to a local port ──
//   int bind(int sockfd, uint32 ip, uint16 port)
uint64
sys_bind(void)
{
  int sockfd, idx;
  uint32 ip_addr;
  int port;               // must be int: argint writes 32 bits!
  struct file *f;

  argint(0, &sockfd);
  argint(1, (int*)&ip_addr);  // IP as uint32 in network byte order
  argint(2, &port);

  if (sockfd < 0 || sockfd >= NOFILE)
    return -1;

  struct proc *p = myproc();
  f = p->ofile[sockfd];
  if (f == 0 || f->type != FD_SOCK)
    return -1;

  idx = f->sockid;
  if (idx < 0 || idx >= NSOCK || !socktab[idx].used)
    return -1;

  acquire(&socktab[idx].lock);
  socktab[idx].lport = port;
  release(&socktab[idx].lock);

  return 0;
}

// ── sys_sendto: send UDP datagram ──
//   int sendto(int sockfd, char *buf, int len,
//              uint32 dst_ip, uint16 dst_port)
uint64
sys_sendto(void)
{
  int sockfd, len, idx;
  uint32 dst_ip;
  int dst_port;          // must be int: argint writes 32 bits!
  struct file *f;
  static char buf[1500];

  argint(0, &sockfd);
  uint64 buf_addr;
  argaddr(1, &buf_addr);
  argint(2, &len);
  argint(3, (int*)&dst_ip);
  argint(4, &dst_port);

  if (sockfd < 0 || sockfd >= NOFILE)
    return -1;
  if (len < 0 || len > (int)sizeof(buf))
    return -1;

  struct proc *p = myproc();
  f = p->ofile[sockfd];
  if (f == 0 || f->type != FD_SOCK)
    return -1;

  idx = f->sockid;
  if (idx < 0 || idx >= NSOCK || !socktab[idx].used)
    return -1;

  // Copy data from user space
  if (copyin(p->pagetable, buf, buf_addr, len) < 0)
    return -1;

  uint16 sport = socktab[idx].lport;
  int ret = udp_send(dst_ip, dst_port, sport, buf, len);
  return ret;
}

// ── sys_recvfrom: receive UDP datagram ──
//   int recvfrom(int sockfd, char *buf, int maxlen,
//                uint32 *src_ip, uint16 *src_port)
//   Returns payload length, -1 on error.
uint64
sys_recvfrom(void)
{
  int sockfd, maxlen, idx;
  struct file *f;
  static char pktbuf[2048];
  uint32 src_ip;
  uint16 src_port, dst_port;

  argint(0, &sockfd);
  uint64 buf_addr;
  argaddr(1, &buf_addr);
  argint(2, &maxlen);
  uint64 ip_addr;
  argaddr(3, &ip_addr);
  uint64 port_addr;
  argaddr(4, &port_addr);

  if (sockfd < 0 || sockfd >= NOFILE)
    return -1;
  if (maxlen < 0 || maxlen > 2048)
    return -1;

  struct proc *p = myproc();
  f = p->ofile[sockfd];
  if (f == 0 || f->type != FD_SOCK)
    return -1;

  idx = f->sockid;
  if (idx < 0 || idx >= NSOCK || !socktab[idx].used)
    return -1;

  // Check if the socket has buffered data
  acquire(&socktab[idx].lock);
  if (socktab[idx].rcount > 0) {
    int avail = socktab[idx].rcount;
    if (avail > maxlen)
      avail = maxlen;
    // Simple linear read from ring buffer
    for (int i = 0; i < avail; i++) {
      pktbuf[i] = socktab[idx].rbuf[socktab[idx].rhead];
      socktab[idx].rhead = (socktab[idx].rhead + 1) % 4096;
    }
    socktab[idx].rcount -= avail;
    // We lose the source info for buffered data, but for a simple
    // implementation, this is acceptable.
    release(&socktab[idx].lock);

    if (copyout(p->pagetable, buf_addr, pktbuf, avail) < 0)
      return -1;
    return avail;
  }
  release(&socktab[idx].lock);

  // No buffered data — poll the NIC until data arrives or timeout
  for (int tries = 0; tries < 1000000; tries++) {
    int n = udp_recv(&src_ip, &src_port, &dst_port, pktbuf, maxlen);
    if (n > 0) {
      // Only deliver if the port matches (or socket is unbound)
      if (socktab[idx].lport == 0 || dst_port == socktab[idx].lport) {
        // Write back source IP and port to user space
        if (copyout(p->pagetable, ip_addr, (char*)&src_ip, sizeof(src_ip)) < 0)
          return -1;
        if (copyout(p->pagetable, port_addr, (char*)&src_port,
                    sizeof(src_port)) < 0)
          return -1;

        if (n > maxlen)
          n = maxlen;
        if (copyout(p->pagetable, buf_addr, pktbuf, n) < 0)
          return -1;

        return n;
      }
      // Port mismatch — ignore and keep polling (packet was consumed)
    }
    // micro delay (~60 µs per iteration on emulated RISC-V)
    for (volatile int j = 0; j < 1000; j++)
      ;
  }
  return -1;  // timeout after ~3 seconds
}

// ── Called by fileclose to clean up socket ──
void
sock_close(int idx)
{
  if (idx < 0 || idx >= NSOCK)
    return;
  acquire(&socktab[idx].lock);
  socktab[idx].used   = 0;
  socktab[idx].rhead  = 0;
  socktab[idx].rtail  = 0;
  socktab[idx].rcount = 0;
  release(&socktab[idx].lock);
}
