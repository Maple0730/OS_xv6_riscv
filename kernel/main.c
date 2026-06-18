#include "types.h"//类型定义
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;
// start() jumps here in supervisor mode on all CPUs.
void main()
{
  // cpuid() 返回当前 CPU 的编号
  if (cpuid() == 0) //是否是0号cpu
  {
    consoleinit();           // 初始化控制台（UART 等）
    printfinit();            // 初始化 printf 系统（如锁）
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();                 // 初始化物理页分配器
    kheapinit();             // 初始化内核堆分配器
    kmalloctest();           // 自测内核堆分配器
    kvminit();               // 创建内核页表（建立虚拟地址映射）
    kvminithart();           // 切换到内核页表（开启分页）
    procinit();              // 初始化进程表，为每个进程分配内核栈
    seminit();               // 初始化信号量表
    monitor_init();          // 初始化管程表
    banker_init_lock();      // 初始化银行家算法自旋锁
    deadlock_init();         // 初始化死锁检测器
    msgq_init();             // 初始化消息队列 (D2)
    trapinit();              // 初始化 trap 数据结构（如中断向量表）
    trapinithart();          // 在当前 hart 上安装 trap 处理函数（设置 stvec）
    plicinit();              // 初始化 PLIC（平台级中断控制器）全局配置
    plicinithart();          // 让当前 hart 能接收 PLIC 设备中断
    binit();                 // 初始化块缓存（buffer cache）
    iinit();                 // 初始化 inode 表
    fileinit();              // 初始化文件表
    virtio_disk_init();      // 初始化虚拟磁盘驱动（virtio）
    userinit();              // 创建第一个用户进程（init 进程）
    // 顺序一致内存屏障：确保此前所有写操作对其它 CPU 可见
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    started = 1;             // 标记初始化完成，唤醒其他 CPU
  } 
  else 
  {
    // 自旋等待 started 变为 1
    while (started == 0)
      ;
    // 内存屏障：确保开始执行下面的代码前，能看到 boot CPU 的所有初始化结果
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    printf("hart %d starting\n", cpuid());
    kvminithart();           // 切换到这个 hart 的内核页表（开启分页）
    trapinithart();          // 安装 trap 处理函数
    plicinithart();          // 允许这个 hart 接收 PLIC 中断
  }

  // 所有 CPU 最终都进入调度器，永不返回
  scheduler();
}
