struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
};

// type 表示这是哪类“打开对象”
  // FD_PIPE：管道
  // FD_INODE：普通文件/目录
  // FD_DEVICE：设备文件
  // ref 引用计数 表示当前有多少 fd 或内核引用共享这个打开文件对象
// readable / writable 当前打开方式是否允许读/写
// pipe 如果这是管道，指向对应 struct pipe
// ip 如果这是普通文件或设备文件，指向对应 inode
// off 当前读写偏移 这是“打开文件实例”的偏移，不是 inode 的全局属性
// major 若是设备文件，记录主设备号

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define mkdev(m, n) ((uint)((m) << 16 | (n)))

// in-memory copy of an inode
struct inode {
  uint dev;              // Device number
  uint inum;             // Inode number
  int ref;               // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;             // inode has been read from disk?

  short type; // copy of disk inode
  short major;//设备用的
  short minor;//设备用的
  short nlink;
  uint size;
  uint addrs[NDIRECT + 1];
};

// map major device number to device functions.
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
