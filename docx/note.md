所以说entry.s是一个把栈区初始化，然后调用start,这个.s文件没有循环是怎么给全部cpu设置栈的，是并行吗，在哪里调用的


• 我先把 entry.S 的实际代码和它与 QEMU/各 hart 的关系对上，然后直接按这份仓库解释“谁调用它、为什么不需要循环、是不是并行”。



• 对，这个理解基本对，但有两个关键点要修正：

  1. entry.S 不是“被某个 C 函数调用”的
  2. 它不给“全部 CPU 逐个设置栈”，而是每个 CPU 都各自执行同一段 entry.S，自己算出自己的栈

  先看代码 xv6-riscv/kernel/entry.S:1：

  # qemu -kernel loads the kernel at 0x80000000
  # and causes each hart (i.e. CPU) to jump there.
  ...
  _entry:
      la sp, stack0
      li a0, 1024*4
      csrr a1, mhartid
      addi a1, a1, 1
      mul a0, a0, a1
      add sp, sp, a0
      call start

  ### 在哪里调用的

  它不是普通“调用”，而是：

  - 链接脚本 xv6-riscv/kernel/kernel.ld:2 指定入口是 _entry

    ENTRY( _entry )

  - 并且把它放在 0x80000000：

    . = 0x80000000;

  - QEMU 用 -kernel 加载后，直接让每个 hart 从这里开始执行

  所以 entry.S 是内核镜像入口点，相当于“CPU 一开机先落到这里”，不是被 main() 或 start() 反过来调用的。

  ### 为什么没有循环也能给所有 CPU 设置栈

  因为不是一个 CPU 在替别的 CPU 干活，而是：

  - 每个 hart 都会执行 _entry
  - 每个 hart 都读取自己的 mhartid
  - 然后根据 mhartid 算出自己该用 stack0 里的哪一段

  stack0 在 xv6-riscv/kernel/start.c:11：

  __attribute__((aligned(16))) char stack0[4096 * NCPU];

  意思是准备了一大块连续内存，里面按 4096 字节切成 NCPU 份，每个 CPU 一份栈。

  entry.S 的计算是：

  sp = stack0 + (mhartid + 1) * 4096

  所以：

  - hart 0 用第 0 份栈顶
  - hart 1 用第 1 份栈顶
  - hart 2 用第 2 份栈顶
  - ...

  注意它取的是“每段末尾”，因为栈向低地址增长。

  ### 是并行吗

  可以认为是并发启动，但不需要你自己写并行逻辑。

  QEMU/硬件把多个 hart 都启动后：

  - 它们都从 _entry 开始跑
  - 各自读自己的 mhartid
  - 各自设置自己的 sp
  - 然后各自 call start

  所以不是：

  - 一个 CPU 循环 for each cpu: set stack

  而是：

  - 每个 CPU 自己执行一次同样的代码

  这是多核启动里很常见的写法。

  ### start 之后发生什么

  在 xv6-riscv/kernel/start.c:15：

  - 设置特权级
  - 设置 mepc = main
  - 关分页
  - 委托中断/异常
  - 配定时器
  - 把 hartid 放进 tp
  - 最后 mret 跳到 main()

  所以链路是：

  QEMU/硬件 -> _entry -> start() -> mret -> main()

  ### 那 entry.S 末尾那个死循环是干嘛的

  spin:
      j spin

  正常情况下到不了这里，因为 call start 之后 start() 不会返回，而是 mret 去 main()。

  这个死循环只是兜底，防止意外返回后 CPU 跑飞。

  一句话总结：

  entry.S 不靠循环给所有 CPU 分栈，而是每个 CPU 都 independently 执行 _entry，根据自己的 mhartid 计算出专属栈，然后进入 start()。

  如果你愿意，我下一条可以给你画一张“3 个 hart 同时经过 entry.S 的时序图”。

───────────────────────────────────────────────────────────────


› 解释一下start.c文件


• start.c 的作用可以概括成一句话：

  它负责把 CPU 从最早期的 machine mode 启动状态，整理成 xv6 内核可用的 supervisor mode 运行环境，然后跳到 main()。

  你可以把它看成：C 语言里的“真正启动代码”。

  文件在 xv6-riscv/kernel/start.c:1。

  ———

  ## 1. 文件里有什么

  最核心的内容就三块：

  - stack0[4096 * NCPU]
  - start()
  - timerinit()

  ———

  ## 2. stack0 是干什么的

  __attribute__((aligned(16))) char stack0[4096 * NCPU];

  这是给 entry.S 用的。

  意思是：

  - 预留一大块栈空间
  - 每个 CPU 一页，4096 字节
  - entry.S 根据 mhartid 给每个 hart 选自己的那一段栈

  所以 start() 能正常用 C 调用栈，前提就是 entry.S 先把 sp 指到 stack0 里的某一段。

  ———

  ## 3. start() 的整体目标

  start() 在 xv6-riscv/kernel/start.c:15：

  void
  start()
  {
    ...
    asm volatile("mret");
  }

  它做的事按顺序是：

  1. 设定将来要切到 S mode
  2. 设定切换后从 main() 开始执行
  3. 暂时关闭分页
  4. 把异常/中断委托给 S mode
  5. 配置 supervisor 能访问物理内存
  6. 开启时钟中断相关设置
  7. 记录当前 hart id
  8. 执行 mret，真正跳到 main()

  ———

  ## 4. 逐段解释

  ### 4.1 设置 mstatus，准备切到 S mode

  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  这里是在设置 mstatus 里的 MPP 字段。

  含义是：

  - 当前还在 M mode
  - 但执行 mret 以后，CPU 应该返回到 S mode

  所以这是在告诉硬件：

  “等会儿 mret 的目标特权级不是 M，而是 S。”

  ———

  ### 4.2 设置 mepc = main

  w_mepc((uint64)main);

  mepc 可以理解成：

  - mret 之后跳去执行的地址

  这里设成 main，所以后面 mret 一执行，CPU 就跳进 xv6-riscv/kernel/main.c:9 的 main()。

  所以不是 start() 直接调用 main()，而是：

  - start() 配寄存器
  - mret 让硬件切模式并跳到 main()

  ———

  ### 4.3 先关闭分页

  w_satp(0);

  satp 是页表控制寄存器。

  这里设成 0，表示：

  - 现在先别开分页
  - 先在最简单的物理地址环境下把启动工作做完

  后面真正建立内核页表是在 main() 里：

  - kvminit()
  - kvminithart()

  ———

  ### 4.4 把异常和中断委托给 S mode

  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE);

  这是非常关键的一步。

  #### w_medeleg(0xffff)

  把异常委托给 supervisor mode 处理。

  #### w_mideleg(0xffff)

  把中断委托给 supervisor mode 处理。

  #### w_sie(...)

  允许 supervisor 接收：

  - 外部中断 SEIE
  - 定时器中断 STIE

  这样后面 xv6 就能主要在 S mode 下处理 trap，而不是每次都回 M mode。

  这正是 xv6 的设计目标：
  内核主要跑在 supervisor mode。

  ———

  ### 4.5 设置 PMP，让 S mode 能访问全部物理内存

  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  PMP 是 Physical Memory Protection。

  这里的作用可以简单理解成：

  给 supervisor mode 开权限，让它可以访问所有物理内存。

  否则即使切到了 S mode，内核也可能没有权限访问它需要的内存区域。

  对学习 xv6 来说，你可以先记住结论，不必一开始深挖 PMP 编码细节。

  ———

  ### 4.6 配定时器中断

  timerinit();

  这是为了让每个 hart 都能收到时钟中断。

  时钟中断后面很重要，因为它关系到：

  - ticks
  - sleep/wakeup
  - yield
  - 时间片调度

  ———

  ## 5. timerinit() 做了什么

  在 xv6-riscv/kernel/start.c:52：

  void
  timerinit()
  {
    w_menvcfg(r_menvcfg() | (1L << 63));
    w_mcounteren(r_mcounteren() | 2);
    w_stimecmp(r_time() + 1000000);
  }

  可以粗略理解成三步：

  1. 允许使用 stimecmp
  2. 允许 supervisor 读时间相关计数器
  3. 设定“下一次时钟中断什么时候发生”

  最后这句：

  w_stimecmp(r_time() + 1000000);

  意思是：

  - 读取当前时间
  - 往后推一段
  - 到那个时刻触发一次 timer interrupt

  后面在 trap.c 的 clockintr() 里会再次设置下一次中断，所以时钟就持续跑起来了。

  ———

  ## 6. 保存 hart id 到 tp

  int id = r_mhartid();
  w_tp(id);

  这里把当前 CPU 的 hart id 写进 tp 寄存器。

  后面 cpuid() 在 xv6-riscv/kernel/proc.c:53 里直接读 tp：

  int
  cpuid()
  {
    int id = r_tp();
    return id;
  }

  所以 start() 这一步是在给以后整个内核的“我现在在哪个 CPU 上”打基础。

  ———

  ## 7. mret 是真正的切换点

  asm volatile("mret");

  这是整个 start.c 最关键的一句。

  它做两件事：

  - 从 M mode 切到前面设好的 S mode
  - 跳到前面设好的 mepc，也就是 main()

  所以启动链是：

  entry.S -> start() -> mret -> main()

  不是：

  entry.S -> start() -> call main()

  ———

  ## 8. 为什么 start() 不直接做所有初始化

  因为 start() 还属于非常早期的启动阶段。

  它只负责：

  - 切模式
  - 配必要寄存器
  - 准备让内核进入正常运行状态

  真正的内核初始化，比如：

  - kinit()
  - kvminit()
  - procinit()
  - trapinit()
  - plicinit()
  - userinit()

  都放在 main() 里做。

  这是一种分层：

  - entry.S：最早期汇编入口
  - start.c：最早期 C 启动
  - main.c：正式内核初始化

  ———

  ## 9. 一句话总结整个文件

  start.c 就是在做这件事：

  把每个 hart 从 QEMU/硬件交给内核的原始启动状态，转换成 xv6 能在 supervisor mode 下运行的状态，并跳到 main() 继续初始化。

  ———

  ## 10. 你现在最该抓住的主线

  记住这 4 步就够了：

  1. entry.S 给每个 CPU 选好栈，并调用 start()
  2. start() 设置好 M mode -> S mode 的返回条件
  3. start() 配置中断委托、PMP、timer、hartid
  4. mret 跳到 main()，内核正式开始初始化

  如果你愿意，我下一条可以继续按同样方式给你讲 main.c，把 start.c -> main.c -> scheduler() 这一段彻底串起来。



› mret是什么东西


• mret 是 RISC-V 的一条特权指令，全名可以理解成：

  Machine-mode return

  也就是：

  从 machine mode 的 trap/异常处理流程里返回。

  在 xv6 的启动里，它被拿来做一件很关键的事：

  不是“返回到原函数”，而是“按照寄存器里提前配置好的状态，切换特权级并跳到指定地址继续执行”。

  ———

  ## 在你的 start.c 里它做了什么

  在 xv6-riscv/kernel/start.c:47：

  w_mepc((uint64)main);
  ...
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);
  ...
  asm volatile("mret");

  这里的意思是：

  1. 先把 mepc 设成 main
  2. 再把 mstatus.MPP 设成 S mode
  3. 执行 mret

  于是 CPU 会：

  - 把当前特权级从 M mode 切到 S mode
  - 把 PC 设为 mepc
  - 也就是跳到 main()

  所以这里 mret 的效果就是：

  “退出 machine mode，进入 supervisor mode，并从 main() 开始执行。”

  ———

  ## mret 依赖哪两个最重要的寄存器

  ### 1. mepc

  Machine Exception Program Counter

  它决定：

  mret 之后跳到哪里执行。

  在 xv6 里被设成：

  w_mepc((uint64)main);

  所以 mret 后跳到 main()。

  ———

  ### 2. mstatus.MPP

  Machine Previous Privilege

  它决定：

  mret 之后要回到哪个特权级。

  在 xv6 里被设成 S：

  x |= MSTATUS_MPP_S;

  所以 mret 后进入 supervisor mode。

  ———

  ## 你可以把它类比成什么

  它有点像“硬件版的跳转 + 模式切换”：

  - 普通 ret：从函数调用返回
  - mret：从 machine-level trap/启动上下文返回，并恢复到指定特权级和地址

  所以它不是普通函数返回指令。

  ———

  ## 为什么 xv6 不直接 call main()

  因为 call main() 只能改 PC，不能完成特权级切换。

  而 xv6 需要的是：

  - 当前在 M mode
  - 之后让内核主要跑在 S mode

  所以必须用 mret 这种特权返回指令。

  ———

  ## 和 sret 的区别

  - mret：从 M mode 返回
  - sret：从 S mode 返回

  在 xv6 里：

  - start.c 用 mret：M -> S
  - trampoline.S / 用户返回路径里会用 sret：S -> U

  所以这两个指令分别服务不同层级的切换。

  ———

  ## 一句话总结

  mret 是 RISC-V 用来从 machine mode 按照 mepc 和 mstatus 恢复执行状态的特权返回指令；在 xv6 启动里，它被用来把 CPU 从 M mode 切到 S mode，并跳到 main()。

  如果你愿意，我下一条可以继续讲：

  - mret 和 sret 的完整对比
  - 为什么 xv6 只在启动阶段用 mret，平时主要用 sret


