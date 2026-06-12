// UDP network test: send and receive datagrams

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Helper: build IPv4 address 10.0.2.x in network byte order.
// e.g. ip(15) = 10.0.2.15, ip(2) = 10.0.2.2 (gateway / host)
static uint32
ip(uint8 host)
{
  return (10 << 24) | (0 << 16) | (2 << 8) | host;
}

int
main(int argc, char *argv[])
{
  int fd, n;
  uint32 src_ip;
  uint16 src_port;
  char buf[512];

  fd = socket(2, 2, 17);  // AF_INET, SOCK_DGRAM, UDP
  if (fd < 0) {
    printf("udptest: socket failed\n");
    exit(1);
  }
  printf("udptest: socket fd=%d\n", fd);

  // Always bind to port 2000
  if (bind(fd, ip(15), 2000) < 0) {
    printf("udptest: bind failed\n");
    close(fd);
    exit(1);
  }

  if (argc >= 2 && strcmp(argv[1], "recv") == 0) {
    // Receive mode: send a tiny "ping" to open NAT mapping,
    // then wait for the host to reply (or send anything).
    char *ping = "ping";
    sendto(fd, ping, strlen(ping), ip(2), 2000);
    printf("udptest: listening on 10.0.2.15:2000 ...\n");
    printf("udptest:   (send to 10.0.2.2:2000 to reach xv6)\n");

    n = recvfrom(fd, buf, sizeof(buf) - 1, &src_ip, &src_port);
    if (n < 0) {
      printf("udptest: timeout (no packet received)\n");
      close(fd);
      exit(1);
    }
    buf[n] = 0;
    printf("udptest: got %d bytes from %d.%d.%d.%d:%d\n",
           n,
           (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
           (src_ip >> 8) & 0xFF, src_ip & 0xFF,
           src_port);
    printf("udptest: data = \"%s\"\n", buf);

  } else {
    // Send mode (default): send a message and wait for reply
    printf("udptest: sending to 10.0.2.2:2000 ...\n");

    char *msg = "Hello from xv6!";
    n = sendto(fd, msg, strlen(msg), ip(2), 2000);
    if (n < 0) {
      printf("udptest: sendto failed\n");
    } else {
      printf("udptest: sent %d bytes\n", n);
    }

    // Wait for reply on the same socket
    printf("udptest: waiting for reply ...\n");
    n = recvfrom(fd, buf, sizeof(buf) - 1, &src_ip, &src_port);
    if (n > 0) {
      buf[n] = 0;
      printf("udptest: reply from %d.%d.%d.%d:%d = \"%s\"\n",
             (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
             (src_ip >> 8) & 0xFF, src_ip & 0xFF,
             src_port, buf);
    } else {
      printf("udptest: no reply (timeout)\n");
    }
  }

  close(fd);
  exit(0);
}
