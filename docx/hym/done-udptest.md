# xv6-riscv UDP 网络协议栈实现文档

## 概述

在 xv6-riscv 操作系统中实现了基于 **virtio-net MMIO** 网卡驱动的完整 UDP 网络协议栈，包含 Ethernet、ARP、IP、UDP 四层协议，以及 socket 系统调用接口。

---

## 架构图

```
┌──────────────────────────────────────────────────────────┐
│                     User Space                            │
│                                                          │
│   udptest.c ── socket() / bind() / sendto() / recvfrom() │
│                         │                                │
└─────────────────────────┼────────────────────────────────┘
                          │ ecall (RISC-V syscall)
┌─────────────────────────┼────────────────────────────────┐
│                   Kernel Space                            │
│                                                          │
│  ┌──────────────────────▼─────────────────────────────┐ │
│  │            Socket Layer (sysnet.c)                  │ │
│  │  ┌──────────┐ ┌──────────┐ ┌────────┐ ┌─────────┐ │ │
│  │  │sys_socket│ │ sys_bind │ │sendto  │ │recvfrom │ │ │
│  │  └────┬─────┘ └────┬─────┘ └───┬────┘ └────┬────┘ │ │
│  │       │            │           │            │       │ │
│  │       │    socktab[NSOCK] ← FD_SOCK 文件描述符      │ │
│  └───────┼────────────┼───────────┼────────────┼──────┘ │
│          │            │           │            │         │
│  ┌───────▼────────────▼───────────▼────────────▼──────┐ │
│  │           Protocol Layer (net.c)                    │ │
│  │                                                    │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────────┐ │ │
│  │  │ udp_send │  │ udp_recv │  │   ARP Engine     │ │ │
│  │  │ 构建全包  │  │ 解包分发  │  │ request/reply   │ │ │
│  │  │ Eth+IP+  │  │ Eth→IP→  │  │ lookup/add_table │ │ │
│  │  │ UDP+data │  │ UDP→user │  └────────┬─────────┘ │ │
│  │  └────┬─────┘  └────┬─────┘           │           │ │
│  └───────┼─────────────┼─────────────────┼───────────┘ │
│          │             │                 │              │
│  ┌───────▼─────────────▼─────────────────▼───────────┐ │
│  │         Driver Layer (virtio_net.c)                │ │
│  │                                                    │ │
│  │  ┌────────────────┐    ┌──────────────────┐       │ │
│  │  │   RX Queue 0   │    │    TX Queue 1    │       │ │
│  │  │  8 descriptors │    │  8 descriptors   │       │ │
│  │  │  WRITE (Dev)   │    │  READ (Dev)      │       │ │
│  │  └───────┬────────┘    └────────┬─────────┘       │ │
│  └──────────┼──────────────────────┼─────────────────┘ │
│             │                      │                    │
│        VIRTIO1 MMIO            VIRTIO1 MMIO             │
│        0x10002000             0x10002000                │
└─────────────┼──────────────────────┼────────────────────┘
              │                      │
     ┌────────▼──────────────────────▼──────────┐
     │           QEMU virtio-net-device           │
     │           (virtio-mmio-bus.1)              │
     └───────────────────┬───────────────────────┘
                         │
     ┌───────────────────▼───────────────────────┐
     │          QEMU SLIRP (User Mode)            │
     │                                            │
     │   10.0.2.15 (Guest/xv6) ←→ 10.0.2.2 (Host)│
     └───────────────────┬───────────────────────┘
                         │
     ┌───────────────────▼───────────────────────┐
     │           Host OS (VMware VM)               │
     │           nc -u -l 2000                    │
     └────────────────────────────────────────────┘
```

---

## 数据包流动路径

```
发送 (sendto):
  User → sys_sendto → udp_send() → ARP lookup/resolve
    → 构建 [Eth(14) + IP(20) + UDP(8) + payload]
    → virtio_net_transmit() → TX virtqueue → QEMU virtio-net → SLIRP → Host

接收 (recvfrom):
  Host → SLIRP → QEMU virtio-net → RX virtqueue (device writes)
    → virtio_net_receive() → udp_recv() → parse [Eth→IP→UDP→payload]
    → sys_recvfrom → User

ARP 解析 (自动触发):
  udp_send 发现目标 MAC 未知 → arp_send_request (广播)
    → gateway (10.0.2.2) 回复 ARP reply → arp_add 记录
```

---

## 一、硬件驱动层 — virtio-net MMIO

### 1.1 为什么选择 virtio-net 而非 E1000

| 方面 | E1000 (PCIe) | virtio-net (MMIO) |
|------|-------------|-------------------|
| 传输方式 | PCIe DMA (descriptor rings) | 共享内存 virtqueue |
| 依赖 | PCI ECAM 枚举 + MSI-X/INTx | 直接 MMIO 访问 |
| QEMU RISC-V 支持 | **PCIe DMA 有 bug**（DD 位永远不置） | 完全可用 |
| 复杂度 | 高（PCI BAR 映射、MSI-X 配置） | 低（kvmmap 一页即可） |

在 QEMU 8.2.2 的 RISC-V `virt` 机器上，通过 GPEX PCIe 主机桥的 DMA 写入存在兼容性问题。E1000 发送描述符的 DD (Descriptor Done) 位永远不被 QEMU 写入，导致发送超时。**virtio-net 使用共享内存通信**，完全避免了 PCIe DMA 问题。

### 1.2 virtio MMIO 内存布局

QEMU 8.2.2 `-machine virt` 的 virtio MMIO 槽位（`force-legacy=false`）：

```
Address      Bus Slot   IRQ   Usage
0x10001000   bus.0      1     virtio-blk  (磁盘, VIRTIO0)
0x10002000   bus.1      2     virtio-net  (网卡, VIRTIO1)
0x10003000   bus.2      3     (未使用)
...
0x10008000   bus.7      8     (未使用)
```

内核页表映射（`vm.c`）：
```c
kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);  // 磁盘
kvmmap(kpgtbl, VIRTIO1, VIRTIO1, PGSIZE, PTE_R | PTE_W);  // 网卡
```

### 1.3 virtio 设备初始化流程

```
1. 读取 MMIO 寄存器验证设备:
   - Magic Value:  0x74726976 ("virt" in ASCII)
   - Version:      2
   - Device ID:    1 (VIRTIO_ID_NET)
   - Vendor ID:    0x554d4551 ("QEMU" in ASCII)

2. 设备状态机 (VIRTIO_MMIO_STATUS):
   ACKNOWLEDGE (1) → DRIVER (2) → FEATURES_OK (8) → DRIVER_OK (4)
                                          ↑
                              negotiate features (全部关闭，最简模式)

3. 初始化 RX virtqueue (Queue 0):
   - kalloc() 分配 descriptor table / avail ring / used ring
   - 8 个 buffer 全部设为 VRING_DESC_F_WRITE
   - 预填充到 avail ring，设备收到包时直接 DMA 到这些 buffer

4. 初始化 TX virtqueue (Queue 1):
   - kalloc() 分配 descriptor table / avail ring / used ring
   - 所有 TX descriptor 初始为空闲状态

5. 读取 MAC 地址:
   - 从 VIRTIO_MMIO_CONFIG + offset(0~5) 读取 6 字节 MAC
```

### 1.4 数据收发流程

**发送 (virtio_net_transmit)**:
```
1. 找空闲 TX descriptor (tx_free[idx])
2. 将数据拷贝到 tx_bufs[idx]，前面预留 virtio_net_hdr (10 字节)
3. 设置 descriptor: addr, len, flags=0 (device reads)
4. 添加到 avail ring，idx++
5. 向 VIRTIO_MMIO_QUEUE_NOTIFY 写入 1 通知设备
6. 轮询等待传输完成
```

**接收 (virtio_net_receive)**:
```
1. 检查 used ring 是否有新条目 (rx_used_idx < rx_used->idx)
2. 读取 pkt_len，跳过头部的 virtio_net_hdr
3. 拷贝以太网帧到 buf
4. 回收 descriptor 到 avail ring
```

---

## 二、协议层 — Ethernet / ARP / IP / UDP

### 2.1 报文格式

```
┌─────────────────────────────────────────────────────────┐
│ Ethernet Header (14 bytes)                              │
│  dst[6] | src[6] | ethertype[2]                         │
├─────────────────────────────────────────────────────────┤
│ IP Header (20 bytes, no options)                        │
│  ver/ihl | tos | tot_len | id | frag_off | ttl |       │
│  protocol(17=UDP) | checksum | src[4] | dst[4]          │
├─────────────────────────────────────────────────────────┤
│ UDP Header (8 bytes)                                    │
│  sport[2] | dport[2] | len[2] | checksum[2]            │
├─────────────────────────────────────────────────────────┤
│ Payload (0 ~ 1472 bytes)                                │
│  （用户数据）                                            │
└─────────────────────────────────────────────────────────┘
```

加上 virtio-net header (10 bytes)，总帧不超过 2048 bytes（NET_BUF_SZ）。

### 2.2 ARP 地址解析

```
ARP 表: arp_table[16]，以 (IP, MAC) 键值对存储

发送流程:
  1. arp_lookup(dst_ip) 查找目标 MAC
  2. 未命中 → arp_send_request() 广播 ARP 请求
  3. 轮询 net_rx_loop() 等待 ARP 回复
  4. 最多重试 2000 次（约 2 秒）
  5. 超时返回 -1

ARP 请求 (broadcast FF:FF:FF:FF:FF:FF):
  Eth: [FF:FF:FF:FF:FF:FF | our_mac | 0x0806]
  ARP: htype=1 ptype=0x0800 oper=1 (request)
       sha=our_mac spa=our_ip
       tha=00:00:00:00:00:00 tpa=target_ip

ARP 回复 (unicast):
  Eth: [requester_mac | our_mac | 0x0806]
  ARP: htype=1 ptype=0x0800 oper=2 (reply)
       sha=our_mac spa=our_ip
       tha=requester_mac tpa=requester_ip
```

### 2.3 IP 校验和

```c
// 16-bit 反码求和（RFC 791）
static uint16 checksum(void *data, uint len) {
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
```

仅计算 IP 头校验和，UDP 校验和设为 0（禁用）。

---

## 三、Socket 系统调用层

### 3.1 新增系统调用

| 系统调用 | 编号 | 函数签名 | 说明 |
|---------|------|---------|------|
| `socket` | 23 | `int socket(domain, type, protocol)` | 创建 UDP socket |
| `bind` | 24 | `int bind(sockfd, ip, port)` | 绑定本地端口 |
| `sendto` | 25 | `int sendto(sockfd, buf, len, dst_ip, dst_port)` | 发送 UDP 包 |
| `recvfrom` | 26 | `int recvfrom(sockfd, buf, maxlen, src_ip, src_port)` | 接收 UDP 包 |

### 3.2 socket 结构体

```c
#define NSOCK 16  // 最多同时 16 个 socket

struct socket {
    int   used;           // 是否已分配
    int   fd;             // 文件描述符编号
    uint32 raddr;         // 远端 IP
    uint16 lport;         // 本地端口 (host byte order)
    uint16 rport;         // 远端端口
    char  rbuf[4096];     // 接收环形缓冲区
    int   rhead, rtail;   // 环形缓冲区读写指针
    int   rcount;         // 缓冲区中字节数
    struct spinlock lock;
};
```

### 3.3 文件类型扩展

```c
// file.h
enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE, FD_SOCK } type;

struct file {
    ...
    int sockid;  // FD_SOCK: 指向 socktab 的索引
};
```

socket 复用 xv6 的文件描述符系统，通过 `FD_SOCK` 类型区分。`fileclose()` 时自动调用 `sock_close()` 释放资源。

### 3.4 sendto 完整路径

```
user/udptest.c
  sendto(fd, "Hello from xv6!", 15, 10.0.2.2, 2000)
    ↓ ecall
kernel/sysnet.c: sys_sendto()
  → copyin(buf) from user space
  → udp_send(dst_ip, dport, sport, buf, len)
    ↓
kernel/net.c: udp_send()
  → arp_lookup(dst_ip) → 未命中 ↓
  → arp_send_request(dst_ip) → virtio_net_transmit(ARP包)
  → net_rx_loop() 等待
  → arp_recv() 处理 ARP 回复
  → 构建 [Eth + IP + UDP + payload]
  → virtio_net_transmit(完整包)
    ↓
kernel/virtio_net.c: virtio_net_transmit()
  → 填充 TX virtqueue descriptor
  → 通知 device (QUEUE_NOTIFY)
  → 轮询完成
    ↓
QEMU virtio-net-device → SLIRP → Host (nc -u -l 2000)
```

---

## 四、QEMU 网络配置

### 4.1 Makefile QEMU 参数

```makefile
QEMUOPTS += -device virtio-net-device,netdev=net0,bus=virtio-mmio-bus.1 \
            -netdev user,id=net0
```

关键点：
- `virtio-net-device`：使用 MMIO 而非 PCI 的 virtio 网卡
- `bus=virtio-mmio-bus.1`：**必须显式指定总线槽位**，否则 QEMU 8.2.2 的自动分配可能放置到错误地址
- `-netdev user,id=net0`：SLIRP user-mode 网络，提供 NAT 和 DHCP 服务
- `-global virtio-mmio.force-legacy=false`：使用新地址布局 (0x10001000 起始)

### 4.2 SLIRP 网络拓扑

```
xv6 Guest (10.0.2.15)  ←→  QEMU SLIRP Gateway (10.0.2.2)
                                  ↕
                          Host OS (VMware VM at 192.168.137.131)
```

- **10.0.2.2**：QEMU user-mode 网络的网关/宿主地址（固定）
- **10.0.2.15**：默认 guest IP（从 QEMU DHCP 获取或硬编码）

---

## 五、关键地址汇总

```c
// memlayout.h
#define VIRTIO0          0x10001000    // virtio-disk MMIO
#define VIRTIO0_IRQ      1
#define VIRTIO1          0x10002000    // virtio-net MMIO
#define VIRTIO1_IRQ      2

// virtio.h
#define VIRTIO_MMIO_MAGIC_VALUE     0x000  // 0x74726976
#define VIRTIO_MMIO_VERSION         0x004  // 2
#define VIRTIO_MMIO_DEVICE_ID       0x008  // 1 = net
#define VIRTIO_MMIO_QUEUE_NOTIFY    0x050
#define VIRTIO_MMIO_STATUS          0x070
#define VIRTIO_MMIO_CONFIG          0x100  // MAC 等配置

// 设备配置
#define VIRTIO_ID_NET  1
#define VIRTIO_ID_BLK  2
```

---

## 六、源代码文件清单

| 文件 | 行数 | 功能 |
|------|------|------|
| `kernel/virtio_net.c` | ~348 | virtio-net MMIO 网卡驱动 |
| `kernel/virtio.h` | ~127 | virtio 协议定义（描述符、寄存器偏移、feature bits） |
| `kernel/net.h` | ~86 | 网络协议头定义（Eth/ARP/IP/UDP 结构体） |
| `kernel/net.c` | ~336 | 协议栈实现（ARP 表、校验和、UDP 收发） |
| `kernel/sysnet.c` | ~286 | Socket 系统调用（socket/bind/sendto/recvfrom） |
| `user/udptest.c` | ~84 | 用户态网络测试程序 |
| `kernel/file.h` | - | 添加 `FD_SOCK` 类型和 `sockid` 字段 |
| `kernel/defs.h` | - | 添加所有网络函数声明 |
| `kernel/memlayout.h` | - | 添加 `VIRTIO1`、`VIRTIO1_IRQ` |
| `kernel/vm.c` | - | 添加 `VIRTIO1` 内核页表映射 |
| `kernel/main.c` | - | 启动时调用 `virtio_net_init()` |
| `kernel/trap.c` | - | 中断处理路由到 `virtio_net_intr()` |
| `kernel/plic.c` | - | PLIC 启用 `virtio_net_irq()` 中断 |
| `Makefile` | - | 编译 `virtio_net.o`、QEMU 网络参数 |

---

## 七、调试历程与关键教训

### 7.1 E1000 PCIe DMA 故障

**现象**：E1000 发送后 TDH 递增但 DD 位始终为 0（设备不写回完成状态）。

**调试过程**：
1. 确认 PCI 枚举和 BAR 映射正确（MMIO read/write 正常）
2. 确认 Bus Mastering 已使能（PCI cmd reg = 0x7）
3. 读取 QEMU 源码确认 descriptor 格式匹配
4. 逐字节 dump descriptor ring，确认 raw data 正确写入
5. DD 位（offset 12-15）始终为 0x00000000

**根因**：QEMU 8.2.2 RISC-V virt 机器的 GPEX PCIe 主机桥在 DMA 写回时有 bug，`pci_dma_write` 对某些地址静默失败。

**解决方案**：放弃 E1000，改用 virtio-net MMIO。

### 7.2 virtio MMIO 地址定位

**问题**：VIRTIO1 初始设为 `0x10001200`（以为紧挨 VIRTIO0），实际应设为 `0x10002000`。

**解决**：通过 `-machine virt,dumpdtb=` 导出设备树（DTB），解析 `virtio_mmio@XXXXXXXX` 节点获得正确的 MMIO 地址。

```bash
# 导出并解析 DTB
qemu-system-riscv64 -machine virt,dumpdtb=/tmp/virt.dtb -bios none \
    -global virtio-mmio.force-legacy=false ...
python3 -c "..." # 解析 virtio_mmio 地址
```

### 7.3 总线槽位显式指定

**问题**：`-device virtio-net-device,netdev=net0` 不加 `bus=` 参数时，QEMU 8.2.2 自动分配的位置不可靠（可能分配到 devid=0 的无效槽位）。

**解决**：显式指定 `bus=virtio-mmio-bus.1`。

### 7.4 RISC-V 指针转换警告

**问题**：`*(volatile uint8 *)(VIRTIO1 + offset)` 在 64-bit RISC-V 上触发 `int-to-pointer-cast`。

**解决**：显式中间转换 `*(volatile uint8 *)(uint64)(VIRTIO1 + offset)`。

---

## 八、测试指南

### 环境准备

启动两个终端，都连接到 xv6 所在的 VMware VM (192.168.137.131)：

```bash
# 终端 1: 宿主机监听 UDP 2000 端口
nc -u -l 2000

# 终端 2: 编译并启动 xv6
cd /home/houyiming/桌面/OS_xv6_riscv
make clean && make
make qemu
```

### 测试 1: xv6 发送 → 宿主机接收（默认模式）

在 xv6 shell 中：
```
udptest
```

**预期输出** (xv6):
```
udptest: socket fd=3
udptest: sending to 10.0.2.2:2000 ...
net: ARP who-has 10.0.2.2
net: ARP request sent for 10.0.2.2, ret=42
net: ARP recv from IP 10.0.2.2 MAC 52:55:a:0:2:2
net: ARP resolved after 0 tries
udptest: sent 15 bytes
udptest: waiting for reply ...
```

终端 1 的 `nc -u -l 2000` 收到 `Hello from xv6!`。

此时 `udptest` 进入阻塞等待状态（~3 秒），在终端 1 中输入回复消息并回车即可发送给 xv6。

### 测试 2: 宿主机回复 → xv6 接收

**在测试 1 的基础上**：终端 1 收到 `Hello from xv6!` 后，输入回复并按回车：

```bash
# 终端 1 (nc -u -l 2000):
Hello from xv6!           # ← xv6 发来的
Hello from host!          # ← 手动输入回复
```

xv6 终端显示：
```
udptest: reply from 10.0.2.2:2000 = "Hello from host!"
```

### 测试 3: 接收模式（发送 ping 后等待）

在 xv6 shell 中：
```
udptest recv
```

xv6 先发送 "ping" 到宿主机来建立 NAT 映射，然后阻塞等待。终端 1 收到 ping 后，输入消息回车即可：

```bash
# 终端 1 (nc -u -l 2000):
ping                       # ← xv6 的 ping
Hello from host!           # ← 手动输入回复
```

xv6 终端显示：
```
udptest: listening on 10.0.2.15:2000 ...
udptest:   (send to 10.0.2.2:2000 to reach xv6)
udptest: got 17 bytes from 10.0.2.2:xxxx
udptest: data = "Hello from host!"
```

### 通信原理

```
xv6 (10.0.2.15:2000) ──sendto──→ 10.0.2.2:2000 ──→ Host nc -u -l 2000
                                                         │
xv6 recvfrom (poll 3s) ←──────────────────── Host 输入回复
```

> **关键点**：QEMU 8.2.2 的 SLIRP 不支持 `hostfwd=udp`（仅 TCP 支持），但支持 **UDP NAT 回复路径**。因此 xv6 需要先发包到宿主机来建立 NAT 映射，宿主机才能回复。

---

## 九、已知限制与改进方向

| 限制 | 改进方案 |
|------|---------|
| 无 UDP 校验和 | 实现 pseudo-header + UDP data 校验和 |
| ARP 表无超时 | 添加 TTL 过期和定期清理 |
| socket 无 select/poll | 实现非阻塞模式和等待队列 |
| 仅支持 UDP | 添加 TCP 支持（需要更复杂的重传/窗口机制） |
| 单包最大 ~1472 bytes | 实现 IP 分片或 Jumbo Frame |
| SLIRP 无法外部访问 | 使用 QEMU tap 网络桥接 |
| 无 DHCP | 硬编码 IP 或实现 DHCP 客户端 |

---

*文档最后更新: 2026-06-12*
