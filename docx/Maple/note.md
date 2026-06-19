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

  › 我对于main.c函数的理解是初始化使得0号cpu进入第一个用户进程然后全部进入调度器，


• 你的理解大方向是对的，但要修正两点。

  更准确的说法是：

  main.c 的作用是：

  1. 让 0 号 CPU 完成一次性的全局初始化
  2. 让其他 CPU 等待这些初始化完成后，再做各自的本地初始化
  3. 创建“第一个用户进程的壳子”
  4. 最后让 所有 CPU 都进入调度器 scheduler()

  但它不会直接让 0 号 CPU 进入第一个用户进程。
  真正“进入第一个用户进程”是后面调度发生的事。

  ———

  ## 你这份代码里的真实流程

  在 xv6-riscv/kernel/main.c:9 里：

  ### 1. 0 号 CPU 做全局初始化

  包括：

  - kinit()：物理页分配器
  - kvminit()：内核页表
  - procinit()：进程表
  - trapinit() / trapinithart()
  - plicinit() / plicinithart()
  - binit() / iinit() / fileinit()
  - virtio_disk_init()
  - userinit()

  这里最关键的是：

  userinit();

  它的作用不是“立刻运行第一个用户进程”，而是：

  创建第一个可调度进程，并把它标成 RUNNABLE。

  ———

  ### 2. 其他 CPU 等待 0 号 CPU 完成初始化

  其他 CPU 会卡在：

  while (started == 0)
    ;

  等 0 号 CPU 把系统公共部分初始化完。

  然后它们各自再做本地初始化：

  - kvminithart()
  - trapinithart()
  - plicinithart()

  ———

  ### 3. 所有 CPU 最后都进 scheduler()

  scheduler();

  这一点很关键：

  - 不是只有 0 号 CPU 进入调度器
  - 是 所有 CPU 都进入调度器

  之后谁先选到第一个 RUNNABLE 进程，取决于调度时机。

  ———

  ## 一个你需要特别注意的点

  你说：

  > 使得0号cpu进入第一个用户进程

  这个说法不够准确，因为：

  - main() 本身不直接切换到用户进程
  - 它只是把第一个进程准备好
  - 后面是 scheduler() 选中它
  - 然后通过 swtch() 切到它
  - 新进程第一次运行从 forkret() 开始
  - 在你这份代码里，forkret() 里还会 kexec("/init")
  - 最后才回到用户态执行 /init

  所以更完整的链路是：

  main -> userinit -> scheduler -> swtch -> forkret -> kexec("/init") -> userret -> /init

  ———

  ## 再进一步说，谁会运行第一个用户进程？

  从这份代码的逻辑上说：

  不一定必须是 0 号 CPU。

  因为：

  - 0 号 CPU先完成初始化
  - 然后它进入 scheduler()
  - 其他 CPU 之后也会进入 scheduler()

  通常最先跑到第一个 RUNNABLE 进程的，大概率是 0 号 CPU，但这不是 main.c 语义上硬编码保证的重点。
  重点是：系统把第一个进程放进可调度队列，然后由调度器决定谁来运行它。

  ———

  ## 你可以把 main.c 总结成这句话

  main.c 负责把系统公共资源初始化好、创建第一个可运行进程、唤醒其他 CPU，然后让所有 CPU 进入调度循环。

  如果你愿意，我可以下一条继续按这个思路，把 xv6-riscv/kernel/main.c:9 逐行给你讲一遍。


› userinit 不也是调用了forkret函数吗


• 不是。userinit() 没有直接调用 forkret()。

  这是两个层次：

  - userinit()：创建并初始化第一个进程
  - forkret()：这个进程第一次被调度运行时，实际进入的内核入口

  关键在这里：

  在 xv6-riscv/kernel/proc.c:146 的 allocproc() 里：

  p->context.ra = (uint64)forkret;

  这句的意思不是“现在调用 forkret”，而是：

  将来这个进程第一次被 swtch() 切进去时，返回地址要落到 forkret。

  所以真实链路是：

  1. main() 调 userinit()
  2. userinit() 调 allocproc()
  3. allocproc() 把新进程的 context.ra 设成 forkret
  4. userinit() 把进程状态设成 RUNNABLE
  5. main() 最后进入 scheduler()
  6. scheduler() 选中这个进程
  7. swtch(&c->context, &p->context) 切到它
  8. 因为 p->context.ra = forkret，所以它第一次开始执行时进入 forkret()

  所以要区分：

  - 直接调用：像 foo() 这种语法
  - 以后切换过去时从那里开始执行：这是 forkret

  userinit()做的是后者的准备，不是前者。

  你可以把它理解成：

  userinit() 负责“把舞台搭好”，scheduler + swtch 才真正“把演员送上台”，而演员上台后的第一句台词就是 forkret()。

  如果你愿意，我可以下一条专门给你画清楚 userinit -> allocproc -> scheduler -> swtch -> forkret 这条控制流。


› kernel.ld只在我宿主机编译的时候运行，指导编译顺序？那和makedile的区别


• 对，kernel.ld 只在宿主机构建阶段起作用，但更准确地说：

  它不是“指导编译顺序”，而是“指导链接结果”。

  这是它和 Makefile 的根本区别。

  ## 一句话区分

  - Makefile：决定怎么构建
  - kernel.ld：决定构建出来的内核长什么样、放在哪里

  ———

  ## Makefile 干什么

  Makefile 管的是构建流程，比如：

  - 先把 entry.S 编译成 entry.o
  - 再把 start.c 编译成 start.o
  - 再把所有 .o 链接成 kernel/kernel
  - 运行 objdump
  - 生成 fs.img
  - 启动 QEMU

  比如这里：

  $K/kernel: $(OBJS) $K/kernel.ld
  	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS)

  意思是：

  - 生成 kernel/kernel 需要这些 .o
  - 也需要 kernel.ld
  - 然后调用 ld 去链接

  所以 Makefile 关心的是：

  - 依赖关系
  - 构建命令
  - 哪一步先做哪一步
  - 文件变了要不要重建

  ———

  ## kernel.ld 干什么

  kernel.ld 只在“链接这一步”被 ld 读取。它管的是：

  - 程序入口是谁：ENTRY(_entry)
  - 内核链接到哪个地址：0x80000000
  - .text/.data/.bss 怎么排布
  - trampoline 放哪
  - etext、end 这些符号定义在哪里

  所以它关心的是：

  - 地址布局
  - 段布局
  - 入口点
  - 链接出来的二进制内存形态

  ———

  ## 它不是“编译顺序说明书”

  你说“指导编译顺序”这个理解差了一点。

  因为：

  - .c -> .o 的编译顺序主要由 Makefile 和依赖决定
  - kernel.ld 不管怎么把 .c 编成 .o
  - 它只在最后 .o -> kernel 时生效

  不过它确实会影响链接时的放置顺序，比如：

  kernel/entry.o(_entry)
  *(.text .text.*)

  这表示：

  - 把 entry.o 里的 _entry 放在最前面
  - 再放其他代码

  这叫链接布局顺序，不是编译顺序。

  ———

  ## 类比一下

  可以把它们比作盖房子：

  - Makefile：施工计划表
    先运砖、再浇地基、再装门窗

  - kernel.ld：建筑设计图
    客厅在哪，楼梯在哪，门朝哪开

  两者都重要，但职责完全不同。

  ———

  ## 再用 xv6 的场景说一遍

  ### Makefile 负责

  - 调 gcc 编译 entry.S, start.c, main.c...
  - 调 ld 链接
  - 把 kernel.ld 传给 ld
  - 最后启动 QEMU

  ### kernel.ld 负责

  - 让 _entry 成为入口
  - 让内核从 0x80000000 开始
  - 让 trampoline 独占一页
  - 让 end 成为内核镜像结束位置

  ———

  ## 结论

  Makefile 决定“怎么生成内核”，kernel.ld 决定“生成的内核在内存里是什么布局”。

  如果你愿意，我下一条可以继续讲一个很实用的问题：

  为什么 entry.S 既要在 Makefile 的 OBJS 里出现，又要在 kernel.ld 里专门放到最前面？


› 解释memlayout.h


• memlayout.h 的作用是：

  定义 xv6 看到的“物理内存和关键地址常量”的地图。

  它不写逻辑，主要是告诉内核：

  - 设备寄存器在哪
  - RAM 从哪开始
  - 内核认为可用内存到哪结束
  - trampoline 和内核栈在虚拟地址空间的哪儿

  文件在 xv6-riscv/kernel/memlayout.h:1。

  ———

  ## 1. 前半部分：物理地址布局

  文件开头先写了 QEMU virt 机器的大致物理地址分布：

  - 0x1000：boot ROM
  - 0x02000000：CLINT
  - 0x0C000000：PLIC
  - 0x10000000：UART
  - 0x10001000：virtio disk
  - 0x80000000：QEMU 把内核加载到这里

  这部分的意义是：

  内核后面访问设备时，需要知道这些 MMIO 寄存器的物理地址。

  所以你会看到这些宏：

  #define UART0     0x10000000L
  #define VIRTIO0   0x10001000
  #define PLIC      0x0c000000L

  它们会被 uart.c、virtio_disk.c、plic.c 使用。

  ———

  ## 2. KERNBASE 和 PHYSTOP

  最重要的是这两句 xv6-riscv/kernel/memlayout.h:39：

  #define KERNBASE 0x80000000L
  #define PHYSTOP  (KERNBASE + 128 * 1024 * 1024)

  ### KERNBASE

  表示内核起始物理地址，也是 QEMU 加载内核的位置。

  也就是：

  - 内核镜像从 0x80000000 开始
  - xv6 认为 RAM 也是从这里开始可用

  ### PHYSTOP

  表示 xv6 打算使用的物理内存上界。

  这里是：

  - 从 0x80000000
  - 到 0x88000000
  - 一共 128MB

  所以这份代码当前只使用 128MB RAM。
  这是当前配置限制，不是 RISC-V 64 位架构本身只能到这。

  ———

  ## 3. TRAMPOLINE

  #define TRAMPOLINE (MAXVA - PGSIZE)

  这表示：

  把 trampoline 那一页映射到用户虚拟地址空间的最高处。

  trampoline 是一小段汇编代码，用来做：

  - 用户态 trap 进入内核
  - 内核返回用户态

  为什么放最高地址？

  - 固定位置，所有进程都一样
  - 切换页表时方便继续执行这段代码
  - 用户页表和内核页表都能映射它

  这是 xv6 trap 设计里的关键技巧。

  ———

  ## 4. TRAPFRAME

  #define TRAPFRAME (TRAMPOLINE - PGSIZE)

  这表示：

  在 trampoline 下方再放一页 trapframe。

  这页用来保存用户寄存器现场。
  当用户态发生系统调用、中断、异常时，先把寄存器存到这里。

  所以用户地址空间高端通常是：

  - 最高一页：TRAMPOLINE
  - 再下一页：TRAPFRAME
  - 再下面：用户真正可用的普通内存

  ———

  ## 5. KSTACK(p)

  #define KSTACK(p) (TRAMPOLINE - ((p) + 1) * 2 * PGSIZE)

  这个宏是给每个进程的内核栈算虚拟地址。

  意思是：

  - 内核栈放在 TRAMPOLINE 下方
  - 每个进程占两页间隔
  - 其中一页是真正栈
  - 另一页是 guard page，不映射

  这样如果内核栈溢出，容易立刻出错，而不是悄悄踩坏别的数据。

  ———

  ## 6. 这份文件和哪些模块直接相关

  ### 和启动相关

  - kernel.ld
  - entry.S
  - main.c

  因为内核从 KERNBASE 开始。

  ### 和内存管理相关

  - vm.c
  - kalloc.c

  因为页表映射和物理页分配都依赖 KERNBASE、PHYSTOP、TRAMPOLINE、KSTACK

  ### 和 trap 相关

  - trap.c
  - trampoline.S
  - proc.c

  因为 TRAMPOLINE 和 TRAPFRAME 是 trap 进出用户态的核心位置。

  ### 和设备驱动相关

  - uart.c
  - virtio_disk.c
  - plic.c

  因为设备 MMIO 地址都在这里定义。

  ———

  ## 7. 你学习时最该抓住的几个常量

  - UART0：串口设备寄存器地址
  - VIRTIO0：虚拟磁盘设备地址
  - PLIC：中断控制器地址
  - KERNBASE：内核/RAM 起点
  - PHYSTOP：xv6 认为可用 RAM 终点
  - TRAMPOLINE：trap 汇编页
  - TRAPFRAME：保存用户寄存器的一页
  - KSTACK(p)：第 p 个进程的内核栈地址

  ———

  ## 8. 一句话总结

  memlayout.h 就是 xv6 的地址地图文件：它把设备、RAM、trampoline、trapframe、内核栈这些关键位置都定义成常量，供启动、页表、trap、驱动和进程管理代码共同使用。

  如果你愿意，我下一条可以直接结合 vm.c 给你讲：这些常量是怎么被真正映射进页表的。


› 为什么 start() 里要设置 mstatus、mepc、satp？
  为什么只有 cpuid() == 0 的 CPU 执行大部分全局初始化？
  main() 结束时为什么所有 CPU 都进入 scheduler()？


• 这三个问题正好对应 xv6 启动的三层逻辑：切模式、分工、进入常态运行。

  ## 1. 为什么 start() 里要设置 mstatus、mepc、satp？

  因为 start() 的任务就是把 CPU 从最初的 machine mode 启动现场，切换成 xv6 需要的 supervisor mode 内核现场。

  ### mstatus

  mstatus 里最关键的是 MPP 字段。start() 把它设成 S，意思是：

  - 当前还在 M mode
  - 但执行 mret 之后，要降到 S mode

  如果不设，mret 后就不会按 xv6 预期进入 supervisor mode。

  ### mepc

  mepc 决定 mret 后跳到哪里执行。

  xv6 里把它设成 main，所以：

  - start() 不是直接 call main()
  - 而是通过 mret 让 CPU 切到 S mode 后，从 main() 开始跑

  ### satp

  satp 是页表控制寄存器。这里先写 0，表示：

  - 启动早期先别开分页
  - 先在最简单的状态下把 supervisor 环境搭起来

  后面到了 main()，才由 kvminit() 和 kvminithart() 建立并启用内核页表。

  一句话说：

  mstatus 决定切到哪个特权级，mepc 决定切过去后从哪里执行，satp 决定此时是否启用页表。

  ———

  ## 2. 为什么只有 cpuid() == 0 的 CPU 执行大部分全局初始化？

  因为这些初始化是全局共享资源初始化，只能做一次。

  比如：

  - kinit()：建立物理页分配器
  - procinit()：初始化全局进程表
  - fileinit()：初始化全局文件表
  - virtio_disk_init()：初始化全局磁盘驱动状态
  - userinit()：创建第一个进程

  这些东西如果每个 CPU 都做一遍，会出问题：

  - 重复初始化同一份全局数据
  - 重复分配资源
  - 互相覆盖状态
  - 甚至直接破坏内核一致性

  所以 xv6 让：

  - CPU 0 负责“一次性全局初始化”
  - 其他 CPU 先等着
  - 等 started = 1 后，再做各自的本地初始化

  本地初始化是每个 CPU 都要做的，比如：

  - kvminithart()
  - trapinithart()
  - plicinithart()

  因为这些和“当前 hart 自己的寄存器/中断入口”有关。

  一句话说：

  全局资源只初始化一次，所以交给 CPU 0；每个 CPU 自己的硬件运行环境仍然要各自初始化。

  ———

  ## 3. main() 结束时为什么所有 CPU 都进入 scheduler()？

  因为完成初始化以后，内核的“常态工作方式”就是：

  每个 CPU 都在调度器循环里，随时准备运行某个进程。

  scheduler() 是每个 CPU 的长期主循环：

  - 找 RUNNABLE 进程
  - 切过去运行
  - 进程让出 CPU 或睡眠后，再回来继续找下一个

  所以 main() 不是内核的长期运行主体，它只是启动阶段。
  启动完成后，CPU 不应该“停在 main 里”，而应该进入系统的正常工作模式，也就是调度。

  为什么是所有 CPU都进去？

  因为 xv6 是多核系统：

  - 每个 CPU 都可以独立调度和运行进程
  - 不可能只有 CPU 0 工作，其他 CPU 闲着

  所以 main() 的终点统一是：

  scheduler();

  这表示：

  - 启动结束
  - 进入 OS 的稳态运行阶段

  一句话说：

  main() 是启动入口，scheduler() 才是内核长期运行的主循环，所以所有 CPU 初始化完后都要进去。

  ———

  ## 你可以把这三件事串成一句话

  start() 负责把 CPU 从启动态切到 xv6 内核态，CPU 0 负责做全局初始化，初始化完成后所有 CPU 都进入 scheduler()，开始系统的正常运行。

  如果你愿意，我下一条可以直接把 xv6-riscv/kernel/main.c:9 按行讲一遍。


kalloc.c解释这个文件


• 我先按你这份 xv6-riscv 的实际 kalloc.c 来讲，不套教材版本；先把文件内容对齐，再按“职责、数据结构、流程”解释。

• Explored
  └ Read kernel.ld, memlayout.h, kalloc.c

──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

• kalloc.c 是 xv6 的物理页分配器。

  一句话概括：

  它负责管理一堆空闲的 4KB 物理页，并提供两个最基本接口：kalloc() 分一页，kfree() 还一页。

  文件在 xv6-riscv/kernel/kalloc.c:1。

  ———

  ## 1. 它管什么，不管什么

  它管的是：

  - 用户进程的物理页
  - 页表页
  - 内核栈对应的物理页
  - pipe buffer 等按页分配的内存

  它不管：

  - 小块内存分配
  - 用户态 malloc
  - 虚拟地址到物理地址的映射策略

  所以它是最底层的“页级物理内存池”。

  ———

  ## 2. 这个文件的核心数据结构

  ### struct run

  struct run {
    struct run *next;
  };

  这个结构很小，只存一个指针。
  它的意思是：

  一页空闲物理页，在空闲时就把这页本身的开头当作链表节点。

  也就是说：

  - 一页正在使用时，它是普通内存
  - 一页空闲时，它被串进空闲链表 freelist

  这是操作系统里很经典的做法：用空闲页自己存管理信息。

  ———

  ### kmem

  struct {
    struct spinlock lock;
    struct run *freelist;
  } kmem;

  这是整个分配器的全局状态：

  - lock：保护空闲链表，防止多核并发分配/释放冲突
  - freelist：空闲页单链表表头

  所以整个分配器本质上就是：

  一个带锁的空闲页单链表。

  ———

  ## 3. kinit()：初始化分配器

  void
  kinit()
  {
    initlock(&kmem.lock, "kmem");
    freerange(end, (void *)PHYSTOP);
  }

  作用：

  1. 初始化自旋锁
  2. 把内核镜像后面到 PHYSTOP 之间的物理内存，全部加入空闲链表

  这里两个边界很关键：

  ### end

  来自 xv6-riscv/kernel/kernel.ld:44

  PROVIDE(end = .);

  表示：

  内核镜像在内存中的结尾地址

  也就是：

  - end 前面已经被内核代码、数据、bss 占用了
  - 这些不能拿来分配

  ### PHYSTOP

  来自 xv6-riscv/kernel/memlayout.h:40

  #define PHYSTOP  (KERNBASE + 128 * 1024 * 1024)

  表示 xv6 认为自己可用 RAM 的上界。

  所以 kinit() 的意思就是：

  把 [end, PHYSTOP) 这一段可用物理内存切成一页一页，放进空闲链表。

  ———

  ## 4. freerange()：批量把一段内存加入空闲链表

  void
  freerange(void *pa_start, void *pa_end)
  {
    char *p;
    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
      kfree(p);
  }

  它做两件事：

  ### 1. 先向上页对齐

  p = (char *)PGROUNDUP((uint64)pa_start);

  因为页分配器按 4KB 页工作。
  如果 pa_start 不是页边界，要先对齐到下一页。

  ### 2. 逐页调用 kfree()

  循环里每次加一个 PGSIZE，把整段内存一页一页回收到空闲链表。

  注意这里虽然叫 kfree()，但这不是“释放之前分配过的页”，而是初始化阶段把可用页登记进分配器。

  ———

  ## 5. kfree()：归还一页物理内存

  void
  kfree(void *pa)

  作用：

  把一页物理内存放回空闲链表。

  ### 先做合法性检查

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  检查三件事：

  1. pa 必须页对齐
  2. pa 不能落在内核镜像内部，也就是不能小于 end
  3. pa 不能超出 xv6 允许管理的物理内存范围

  只要不满足就 panic，因为这说明内核内存管理已经乱了。

  ### 用垃圾值填满整页

  memset(pa, 1, PGSIZE);

  这是调试技巧。
  把释放掉的页全部写成 1，有助于暴露“悬空指针继续使用旧内存”的 bug。

  ### 插入空闲链表表头

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);

  就是典型的头插法：

  - 这页变成一个 run
  - next 指向原来的表头
  - 它自己成为新的表头

  所以 kfree() 时间复杂度是 O(1)。

  ———

  ## 6. kalloc()：分配一页物理内存

  void *
  kalloc(void)

  作用：

  从空闲链表取出一页，返回给内核使用。

  ### 从链表头取一页

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  如果链表非空，就弹出表头。
  如果空了，r == 0，表示没有可用页了。

  ### 用垃圾值填满整页

  if (r)
    memset((char *)r, 5, PGSIZE);

  同样是调试技巧。
  新分配出的页先填成 5，有助于暴露“代码错误依赖旧内容”的 bug。

  ### 返回页首地址

  return (void *)r;

  这就是一页物理内存的起始地址。

  ———

  ## 7. 这个分配器的本质模型

  你可以把它想成：

  - xv6 启动时把所有可用物理页收集起来
  - 串成一个单链表
  - kalloc()：从链表头摘一页
  - kfree()：把一页插回链表头

  所以它是一个非常简洁的：

  “基于空闲页链表的页分配器”

  ———

  ## 8. 它和其他模块怎么配合

  ### 和 kernel.ld

  end 来自链接脚本，告诉它“内核镜像到哪结束”。

  ### 和 memlayout.h

  PHYSTOP 告诉它“最多管理到哪”。

  ### 和 vm.c

  vm.c 建页表时需要页表页，会调用 kalloc()。

  ### 和 proc.c

  进程的 trapframe、内核栈物理页等都依赖 kalloc()。

  ### 和 exec.c

  装载用户程序、扩展用户内存时，底层也要分配物理页。

  ———

  ## 9. 为什么这个实现这么简单

  因为 xv6 是教学系统，它刻意省略了很多真实 OS 的复杂性，比如：

  - 伙伴系统
  - slab/slub
  - NUMA
  - 页着色
  - 内存回收策略
  - 按大小分配的小对象缓存

  xv6 这里只保留最核心思想：

  “物理内存按页管理，空闲页用链表串起来。”

  ———

  ## 10. 你现在最该记住的 5 句话

  1. kalloc.c 管的是物理页，不是任意大小内存块。
  2. 空闲页被组织成一个单链表。
  3. kinit() 把 [end, PHYSTOP) 加入空闲链表。
  4. kalloc() 从表头取一页，kfree() 把一页插回表头。
  5. memset(..., 1/5, PGSIZE) 是为了更容易发现内存错误。

  如果你愿意，我下一条可以继续接着讲：vm.c 是怎么在 kalloc() 之上建立页表的。

  › 解析vm.c


• 我先按你这份 xv6-riscv 的实际 vm.c 来讲，先把文件结构和关键函数对应上，再按“内核页表、用户页表、缺页扩展”三条主线解释。

• Explored
  └ Read vm.c, vm.h, riscv.h


• vm.c 是 xv6 的虚拟内存核心文件。

  一句话概括：

  它负责建立和操作页表，把虚拟地址映射到物理页，并提供内核/用户空间的内存管理基础能力。

  在你这份代码里，它不只是“标准 xv6 的页表文件”，还多了一个明显特征：

  支持 lazy allocation（惰性分配），也就是 vmfault() 这条线。

  文件在 xv6-riscv/kernel/vm.c:1。

  ———

  ## 先给你一个总图

  这个文件可以分成 4 组：

  1. 内核页表
      - kvmmake
      - kvmmap
      - kvminit
      - kvminithart

  2. 页表遍历和映射
      - walk
      - walkaddr
      - mappages

  3. 用户空间内存管理
      - uvmcreate
      - uvmalloc
      - uvmdealloc
      - uvmunmap
      - uvmfree
      - uvmcopy
      - uvmclear
      - freewalk

  4. 内核和用户之间拷贝 + 惰性分配
      - copyout
      - copyin
      - copyinstr
      - vmfault
      - ismapped

  你可以把它理解成：

  - 前半部分负责“页表是什么、怎么建”
  - 后半部分负责“进程内存怎么长、怎么缩、怎么拷贝”

  ———

  # 1. 内核页表主线

  ## kernel_pagetable

  在 xv6-riscv/kernel/vm.c:14：

  pagetable_t kernel_pagetable;

  这是内核共享页表。
  所有 CPU 最终都会切换到它。

  ———

  ## kvmmake()：构造内核页表

  在 xv6-riscv/kernel/vm.c:22。

  它做的事是：

  1. 分配一页作为根页表
  2. 把关键设备地址映射进去
  3. 把内核代码和数据映射进去
  4. 把 trampoline 映射到高地址
  5. 给每个进程预留内核栈映射

  ### 关键映射

  #### UART

  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  表示 MMIO 设备寄存器直接映射。

  #### virtio disk

  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  #### PLIC

  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  #### 内核代码段

  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

  代码段只读可执行，不可写。

  #### 内核数据和可用 RAM

  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

  这一段是直接映射思想的核心：

  - 虚拟地址 = 物理地址
  - 从 etext 往后到 PHYSTOP

  #### trampoline

  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  trap 进出用户态时要用。

  #### 每个进程的内核栈

  proc_mapstacks(kpgtbl);

  ———

  ## kvmmap()

  在 xv6-riscv/kernel/vm.c:59。

  它只是对 mappages() 的一个包装：

  if (mappages(...) != 0)
    panic("kvmmap");

  特点：

  - 只在启动阶段用
  - 失败就直接 panic
  - 用于内核静态映射

  ———

  ## kvminit()

  在 xv6-riscv/kernel/vm.c:67。

  作用很简单：

  kernel_pagetable = kvmmake();

  就是把内核页表造出来。

  ———

  ## kvminithart()

  在 xv6-riscv/kernel/vm.c:75。

  作用是：

  1. sfence_vma()：确保之前页表写入生效
  2. w_satp(MAKE_SATP(kernel_pagetable))：把硬件页表寄存器切到内核页表
  3. 再 sfence_vma()：清 TLB

  这一步之后，当前 CPU 才真正开始使用分页。

  ———

  # 2. 页表遍历与映射主线

  这部分是 vm.c 的核心基础设施。

  ## walk()

  在 xv6-riscv/kernel/vm.c:99。

  作用：

  沿着三级页表找到某个虚拟地址对应的最低级 PTE。

  如果中间缺页表页：

  - alloc == 1：就分配新的页表页
  - alloc == 0：就返回 0

  ### 关键逻辑

  for (int level = 2; level > 0; level--)

  说明 Sv39 是三级页表：

  - level 2
  - level 1
  - level 0

  ### 如果上层页表项已存在

  if (*pte & PTE_V)
    pagetable = (pagetable_t)PTE2PA(*pte);

  说明这个 PTE 指向下一层页表页。

  ### 如果不存在且允许分配

  pagetable = (pde_t *)kalloc();
  memset(pagetable, 0, PGSIZE);
  *pte = PA2PTE(pagetable) | PTE_V;

  也就是现场新建页表页。

  ———

  ## walkaddr()

  在 xv6-riscv/kernel/vm.c:122。

  作用：

  把一个用户虚拟地址翻译成物理地址。

  它要求：

  - va < MAXVA
  - PTE 存在
  - PTE_V 有效
  - PTE_U 置位，说明用户可访问

  如果有任何一步不满足，返回 0。

  这函数常用于 copyin/copyout/copyinstr。

  ———

  ## mappages()

  在 xv6-riscv/kernel/vm.c:147。

  作用：

  建立一段连续虚拟页到连续物理页的映射。

  ### 前提要求

  - va 页对齐
  - size 页对齐
  - size != 0

  ### 每页做的事

  1. walk(..., 1) 找到或创建叶子 PTE
  2. 如果原来已经映射，panic
  3. 写入：

     *pte = PA2PTE(pa) | perm | PTE_V;

  这是所有映射的基础函数。

  ———

  # 3. 用户页表与进程内存管理

  ## uvmcreate()

  在 xv6-riscv/kernel/vm.c:180。

  作用：

  创建一个空的用户根页表。

  就是：

  - kalloc() 一页
  - 清零
  - 返回

  注意这里只是“空页表”，不含用户程序页。

  ———

  ## uvmunmap()

  在 xv6-riscv/kernel/vm.c:194。

  作用：

  删除一段用户页映射。

  参数 do_free 很关键：

  - do_free == 1：映射删掉，物理页也 kfree
  - do_free == 0：只删映射，不回收物理页

  你这个版本对“映射不存在”的处理比原版更宽松：

  if ((pte = walk(...)) == 0) continue;
  if ((*pte & PTE_V) == 0) continue;

  也就是不存在时直接跳过，不 panic。

  这和 lazy allocation 比较相容，因为“地址空间逻辑上有，物理页可能还没真正分配”。

  ———

  ## uvmalloc()

  在 xv6-riscv/kernel/vm.c:218。

  作用：

  把进程用户空间从 oldsz 扩展到 newsz。

  步骤：

  1. 从 PGROUNDUP(oldsz) 开始逐页扩展
  2. 每页 kalloc
  3. 清零
  4. mappages(..., PTE_R | PTE_U | xperm)

  这里 xperm 表示额外权限：

  - 可执行段会带 PTE_X
  - 普通可写数据页会带 PTE_W

  失败时会回滚已分配部分。

  ———

  ## uvmdealloc()

  在 xv6-riscv/kernel/vm.c:249。

  作用：

  把用户空间从 oldsz 缩小到 newsz。

  如果确实跨越页边界，就调用 uvmunmap(..., 1) 回收对应物理页。

  ———

  ## freewalk()

  在 xv6-riscv/kernel/vm.c:265。

  作用：

  递归释放页表本身。

  注意它只释放“页表页”，不负责释放用户数据页。
  所以如果还存在叶子映射，它会 panic：

  else if (pte & PTE_V)
    panic("freewalk: leaf");

  这就是为什么必须先 uvmunmap/uvmfree 把叶子页清掉。

  ———

  ## uvmfree()

  在 xv6-riscv/kernel/vm.c:285。

  作用：

  1. 先释放用户数据页
  2. 再 freewalk() 释放页表层级

  是用户地址空间的完整销毁函数。

  ———

  ## uvmcopy()

  在 xv6-riscv/kernel/vm.c:299。

  作用：

  fork() 时把父进程用户内存复制到子进程。

  步骤：

  1. 遍历 [0, sz) 的每一页
  2. 找到父页表里的 PTE
  3. 如果那页不存在，就 continue
  4. 对已存在页：
      - kalloc() 一个新物理页
      - memmove() 拷过去
      - mappages() 到子页表

  这个版本也适配 lazy allocation：
  如果某页“逻辑上属于地址空间，但还没物理分配”，它会直接跳过，不强制 panic。

  ———

  ## uvmclear()

  在 xv6-riscv/kernel/vm.c:331。

  作用：

  清掉某页的 PTE_U 位。

  这通常用于用户栈下方的 guard page。
  页还映射着，但用户态不能访问，一访问就 fault。

  ———

  # 4. 用户态/内核态数据拷贝

  这几个函数经常和系统调用参数处理一起出现。

  ## copyout()

  在 xv6-riscv/kernel/vm.c:345。

  作用：

  把内核数据拷到用户地址空间。

  比如内核给用户写返回数据。

  ### 特别点

  你的版本里如果地址对应页没映射：

  if (pa0 == 0) {
    if ((pa0 = vmfault(pagetable, va0, 0)) == 0)
      return -1;
  }

  也就是会尝试按需分配该页。

  之后还会检查：

  if ((*pte & PTE_W) == 0)
    return -1;

  防止往只读用户页写。

  ———

  ## copyin()

  在 xv6-riscv/kernel/vm.c:383。

  作用：

  把用户数据拷到内核。

  比如读用户传来的缓冲区。

  如果虚拟页还没真正分配，也会调用 vmfault() 尝试补页。

  ———

  ## copyinstr()

  在 xv6-riscv/kernel/vm.c:412。

  作用：

  从用户空间复制以 \0 结尾的字符串到内核。

  和 copyin() 的区别是：

  - 它按字符找 \0
  - 常用于路径名、命令参数

  注意这个版本里如果 walkaddr() 失败会直接返回 -1，没有走 vmfault()。

  ———

  # 5. 你这份代码最特别的部分：vmfault()

  这是和标准 xv6 区别比较大的地方。

  ## vmfault()

  在 xv6-riscv/kernel/vm.c:455。

  作用：

  当进程访问一个逻辑上属于自己地址空间、但尚未分配物理页的地址时，现场分配并建立映射。

  ### 它做的检查

  #### 1. 不能超过进程大小

  if (va >= p->sz)
    return 0;

  说明这个地址必须在进程合法地址空间范围内。

  #### 2. 对齐到页边界

  va = PGROUNDDOWN(va);

  #### 3. 已经映射过就不处理

  if (ismapped(pagetable, va))
    return 0;

  #### 4. 分配一页物理内存并映射

  mem = (uint64)kalloc();
  memset(..., 0, PGSIZE);
  mappages(..., PTE_W | PTE_U | PTE_R)

  最后返回新分配的物理地址。

  ———

  ## ismapped()

  在 xv6-riscv/kernel/vm.c:478。

  作用很直接：

  检查某个虚拟页当前是否已经有有效映射。

  ———

  # 6. 把整条逻辑串起来

  你可以这样理解整个 vm.c：

  ## 第一层：页表骨架

  - walk
  - mappages
  - walkaddr

  这是“页表操作原语”。

  ## 第二层：地址空间生命周期

  - uvmcreate
  - uvmalloc
  - uvmdealloc
  - uvmunmap
  - uvmfree
  - uvmcopy

  这是“进程内存从创建、增长、复制到销毁”。

  ## 第三层：内核共享地址空间

  - kvmmake
  - kvminit
  - kvminithart

  这是“让整个内核能跑起来”。

  ## 第四层：按需补页

  - vmfault
  - copyin
  - copyout
  - trap.c 里的 page fault 处理

  这是你这份项目相比教材版更值得重点看的变化。

  ———

  # 7. 你现在最该抓住的 8 个函数

  如果只看最核心的，优先顺序我建议是：

  1. kvmmake
  2. walk
  3. mappages
  4. uvmalloc
  5. uvmunmap
  6. uvmcopy
  7. copyin/copyout
  8. vmfault

  这 8 个吃透了，vm.c 基本就立住了。

  ———

  # 8. 一句话总结

  vm.c 是 xv6 的页表与地址空间总控文件：
  上半部分建立内核页表和用户页表，下半部分管理进程内存的分配、复制、释放，并在你这份代码里额外支持 lazy allocation 的缺页补页逻辑。

  如果你愿意，我下一条可以继续专门讲这三个最难混的函数关系：

  - walk
  - walkaddr
  - mappages

  这三个搞明白后，vm.c 会清晰很多。



› 讲解一下riscv.h


• 我先按你这份 xv6-riscv 的实际 riscv.h 来讲，重点抓“寄存器访问、页表相关宏、trap/中断相关位定义”三块。


• riscv.h 可以理解成 xv6 的 RISC-V 硬件说明书 + 访问工具箱。

  一句话概括：

  它把 RISC-V 的关键寄存器、状态位、页表格式、地址转换宏，全都包装成内核能直接调用的 C 宏和内联函数。

  文件在 xv6-riscv/kernel/riscv.h:1。

  ———

  ## 先说这个文件为什么存在

  内核要做很多“直接碰硬件寄存器”的事，比如：

  - 读当前 CPU 编号
  - 设置 trap 入口
  - 打开/关闭中断
  - 切换页表
  - 看 page fault 原因
  - 解析页表项

  这些东西不能靠普通 C 标准库解决，所以 xv6 把它们统一放在 riscv.h。

  你可以把它理解成：

  - proc.c 管进程
  - vm.c 管页表逻辑
  - trap.c 管 trap 流程
  - riscv.h 提供“操作 RISC-V 硬件”的基础指令接口

  ———

  # 一、这个文件可以分成 4 大块

  ## 1. CSR 寄存器读写接口(状态与控制寄存器：Control and Status Register)

  比如：

  - r_mhartid()
  - r_mstatus() / w_mstatus()
  - r_sstatus() / w_sstatus()
  - w_mepc()
  - w_sepc()
  - w_stvec()
  - r_scause()
  - r_stval()
  - w_satp()

  这些函数本质上都是用内联汇编去读写 CSR。

  ———

  ## 2. 中断/特权级相关位定义

  比如：

  - MSTATUS_MPP_*
  - SSTATUS_SPP
  - SSTATUS_SIE
  - SIE_SEIE
  - SIE_STIE

  这些是寄存器里某些 bit 的含义。

  ———

  ## 3. 页表/虚拟内存相关宏

  比如：

  - PGSIZE
  - PGROUNDUP
  - PTE_V/R/W/X/U
  - PA2PTE
  - PTE2PA
  - PX(level, va)
  - MAXVA
  - MAKE_SATP

  这些是 vm.c 的核心基础。

  ———

  ## 4. 少量通用底层工具

  比如：

  - intr_on() / intr_off()
  - intr_get()
  - r_tp() / w_tp()
  - sfence_vma()

  ———

  # 二、第一部分：寄存器访问接口

  这部分你要抓住一个模式：

  - r_xxx() = read，读寄存器
  - w_xxx() = write，写寄存器

  例如：

  static inline uint64
  r_mhartid()
  {
    uint64 x;
    asm volatile("csrr %0, mhartid" : "=r"(x));
    return x;
  }

  意思就是：

  - 执行 RISC-V 指令 csrr
  - 读取 mhartid
  - 返回当前 hart id

  ———

  ## r_mhartid()

  在 xv6-riscv/kernel/riscv.h:4

  作用：

  读取当前 CPU/hart 的编号。

  start.c 里会用它：

  - 给每个 hart 设置自己的 tp
  - entry.S 也会根据 hartid 选栈

  ———

  ## r_mstatus() / w_mstatus()

  在 xv6-riscv/kernel/riscv.h:19

  作用：

  读写 machine mode 的状态寄存器 mstatus。

  start.c 里最重要的用途是设置：

  MSTATUS_MPP_S

  也就是 mret 后要切到 supervisor mode。

  ———

  ## w_mepc()

  在 xv6-riscv/kernel/riscv.h:36

  作用：

  设置 machine exception return 后跳转的地址。

  xv6 启动时把它设成 main。

  ———

  ## r_sstatus() / w_sstatus()

  在 xv6-riscv/kernel/riscv.h:50

  作用：

  读写 supervisor mode 的状态寄存器。

  这在 trap.c 里很常见，比如：

  - 看 trap 是不是来自用户态
  - 开关中断
  - 返回用户态前设置 SPP / SPIE

  ———

  ## w_sepc() / r_sepc()

  在 xv6-riscv/kernel/riscv.h:115

  作用：

  保存/恢复 supervisor trap 返回地址。

  比如在 usertrap()、kerneltrap() 里都会用。

  ———

  ## w_stvec() / r_stvec()

  在 xv6-riscv/kernel/riscv.h:161

  作用：

  设置 trap 入口地址。

  这特别重要：

  - 在内核态 trap 时，stvec = kernelvec
  - 回用户态前，stvec = trampoline 里的 uservec

  所以它决定“下次 trap 进来时先跳到哪段汇编”。

  ———

  ## w_satp() / r_satp()

  在 xv6-riscv/kernel/riscv.h:229

  作用：

  控制当前页表。

  satp 可以理解成“当前地址空间寄存器”。

  - kvminithart() 用它切到内核页表
  - userret 用它切回用户页表

  ———

  ## r_scause() / r_stval()

  在 xv6-riscv/kernel/riscv.h:243

  作用：

  - scause：trap 原因
  - stval：相关附加值，常常是 fault 地址

  比如 page fault 时：

  - r_scause() 看是 load/store page fault
  - r_stval() 看 fault 的虚拟地址

  ———

  # 三、第二部分：状态位和中断位

  这部分不是函数，而是 bit mask。

  ———

  ## MSTATUS_MPP_*

  在 xv6-riscv/kernel/riscv.h:14

  表示 mstatus 里的 MPP 字段：

  - MSTATUS_MPP_M
  - MSTATUS_MPP_S
  - MSTATUS_MPP_U

  作用是指定：

  mret 之后回到哪个特权级。

  xv6 启动时用的是：

  MSTATUS_MPP_S

  ———

  ## SSTATUS_SPP

  在 xv6-riscv/kernel/riscv.h:44

  表示：

  发生 trap 之前，CPU 处于什么特权级。

  - 1 = Supervisor
  - 0 = User

  所以 usertrap() 里会检查：

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic(...)

  因为 usertrap() 预期 trap 来自用户态。

  ———

  ## SSTATUS_SPIE / SSTATUS_SIE

  - SIE：当前 S mode 中断使能
  - SPIE：trap 返回后恢复的中断使能状态

  这两个位在 trap 返回用户态前非常关键。

  ———

  ## SIE_SEIE / SIE_STIE

  在 xv6-riscv/kernel/riscv.h:80

  表示 supervisor 层面允许哪些中断：

  - SEIE：外部中断
  - STIE：定时器中断

  start.c 会设置这些。

  ———

  # 四、第三部分：页表和虚拟内存宏

  这是 vm.c 最依赖的一块。

  ———

  ## PGSIZE 和 PGSHIFT

  在 xv6-riscv/kernel/riscv.h:352

  #define PGSIZE  4096
  #define PGSHIFT 12

  意思：

  - 一页 4096 字节
  - 页内偏移是低 12 位

  ———

  ## PGROUNDUP() / PGROUNDDOWN()

  在 xv6-riscv/kernel/riscv.h:355

  作用：

  - PGROUNDUP(sz)：向上对齐到页边界
  - PGROUNDDOWN(a)：向下对齐到页边界

  它们在 kalloc.c、vm.c、exec.c 里都很常见。

  ———

  ## PTE_V/R/W/X/U

  在 xv6-riscv/kernel/riscv.h:358

  页表项权限位：

  - PTE_V：有效
  - PTE_R：可读
  - PTE_W：可写
  - PTE_X：可执行
  - PTE_U：用户可访问

  这是页表权限控制的核心。

  例如：

  - 用户代码页：R | X | U
  - 用户数据页：R | W | U
  - 内核代码页：R | X
  - trampoline：R | X

  ———

  ## PA2PTE() / PTE2PA()

  在 xv6-riscv/kernel/riscv.h:365

  作用：

  - PA2PTE(pa)：把物理地址编码进页表项格式
  - PTE2PA(pte)：从页表项取出物理地址

  因为页表项不直接原样存物理地址，中间有 bit 布局转换。

  ———

  ## PTE_FLAGS()

  在 xv6-riscv/kernel/riscv.h:369

  从 PTE 中取出低位权限标志。

  uvmcopy() 会用它复制权限。

  ———

  ## PX(level, va)

  在 xv6-riscv/kernel/riscv.h:374

  作用：

  从虚拟地址里取出某一层页表索引。

  Sv39 是三级页表：

  - PX(2, va)
  - PX(1, va)
  - PX(0, va)

  walk() 就靠这个逐层找页表项。

  ———

  ## MAXVA

  在 xv6-riscv/kernel/riscv.h:380

  表示 xv6 允许使用的最高虚拟地址上界。

  注意它不是 Sv39 的理论最大值，而是少一位，避免符号扩展麻烦。

  这个值和 TRAMPOLINE 紧密相关，因为：

  TRAMPOLINE = MAXVA - PGSIZE

  ———

  ## SATP_SV39 和 MAKE_SATP()

  在 xv6-riscv/kernel/riscv.h:223

  作用：

  - 指定页表模式是 Sv39
  - 根据页表根地址生成能写入 satp 的值

  kvminithart() 和 trap 返回路径都依赖它。

  ———

  # 五、第四部分：中断控制和辅助函数

  ———

  ## intr_on() / intr_off() / intr_get()

  在 xv6-riscv/kernel/riscv.h:286

  作用：

  - intr_on()：开中断
  - intr_off()：关中断
  - intr_get()：看当前是否开中断

  本质是改 sstatus 里的 SSTATUS_SIE 位。

  proc.c、trap.c、锁代码里经常用。

  ———

  ## r_tp() / w_tp()

  在 xv6-riscv/kernel/riscv.h:317

  tp 是线程指针寄存器，但 xv6 把它拿来保存：

  当前 hart id

  所以：

  - start.c 用 w_tp(id)
  - cpuid() 用 r_tp()

  这是 xv6 的一个约定。

  ———

  ## r_sp() / r_ra()

  辅助读取：

  - 当前栈指针 sp
  - 返回地址寄存器 ra

  通常用于调试或底层场景。

  ———

  ## sfence_vma()

  在 xv6-riscv/kernel/riscv.h:340

  作用：

  刷新 TLB。

  当页表改了以后，CPU 可能还缓存着旧翻译，所以要执行：

  sfence.vma

  kvminithart() 就会在切换页表前后做这件事。

  ———

  # 六、这个文件和哪些模块直接对应

  ## 和 start.c

  - r_mhartid
  - r_mstatus / w_mstatus
  - w_mepc
  - w_satp
  - w_medeleg
  - w_mideleg
  - w_sie
  - w_tp

  ## 和 trap.c

  - r_sstatus / w_sstatus
  - w_stvec
  - r_scause
  - r_stval
  - w_sepc
  - intr_on/off

  ## 和 vm.c

  - PTE_*
  - PA2PTE
  - PTE2PA
  - PX
  - MAKE_SATP
  - MAXVA
  - PGROUNDUP/DOWN

  ## 和 proc.c

  - r_tp
  - 中断控制函数

  ———

  # 七、你学习时最该先吃透的 12 个名字

  如果现在一次全记住太多，你先抓这 12 个最值钱：

  1. r_mhartid
  2. w_mstatus
  3. w_mepc
  4. r_sstatus
  5. w_stvec
  6. w_satp
  7. r_scause
  8. r_stval
  9. PTE_V
  10. PTE_R/W/X/U
  11. PX(level, va)
  12. MAKE_SATP

  这 12 个吃透了，你看 start.c、trap.c、vm.c 会顺很多。

  ———

  # 八、一句话总结

  riscv.h 不是普通头文件，它是 xv6 对 RISC-V 硬件接口的封装层：
  上半部分负责寄存器和特权控制，下半部分负责页表格式和地址转换宏，给启动、trap、调度、虚拟内存所有核心模块打底。

  如果你愿意，我下一条可以继续专门讲：

  riscv.h 里最重要的那几组寄存器在 xv6 里分别出现在哪些函数里。

• Explored
  └ Read trap.c, start.c
    Search r_mhartid|w_mstatus|w_mepc|r_sstatus|w_stvec|w_satp|r_scause|r_stval|MAKE_SATP|PX\( in kernel

──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

• 可以。riscv.h 里最重要的那几组寄存器，其实都能在 start.c、trap.c、vm.c 里找到落点。这样学会比单看定义清楚很多。

  ———

  ## 1. r_mhartid

  出现在 xv6-riscv/kernel/start.c:44：

  int id = r_mhartid();
  w_tp(id);

  作用是：

  - 启动早期读取当前 hart 编号
  - 存进 tp
  - 后面 cpuid() 直接从 tp 取

  所以它解决的是：

  “我现在是哪颗 CPU？”

  ———

  ## 2. w_mstatus

  出现在 xv6-riscv/kernel/start.c:18：

  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  作用是：

  - 设置 mret 之后回到 S mode
  - 是 xv6 从 M mode 进入 S mode 的前提

  所以它解决的是：

  “等会儿切换特权级时，要切到哪一层？”

  ———

  ## 3. w_mepc

  出现在 xv6-riscv/kernel/start.c:25：

  w_mepc((uint64)main);

  作用是：

  - 设置 mret 之后跳到 main()

  所以它解决的是：

  “切完特权级之后，从哪里开始执行？”

  ———

  ## 4. w_satp / MAKE_SATP

  有两条主线。

  ### 启动阶段

  在 xv6-riscv/kernel/start.c:28：

  w_satp(0);

  表示启动早期先关闭分页。

  ### 内核正式启用页表

  在 xv6-riscv/kernel/vm.c:80：

  w_satp(MAKE_SATP(kernel_pagetable));

  表示把当前 CPU 切换到内核页表。

  ### 返回用户态时

  在 xv6-riscv/kernel/trap.c:91：

  uint64 satp = MAKE_SATP(p->pagetable);

  返回给 trampoline，后面切回用户页表。

  所以这组东西解决的是：

  “CPU 现在应该使用哪个地址空间？”

  ———

  ## 5. w_stvec

  主要在 xv6-riscv/kernel/trap.c:29 和 xv6-riscv/kernel/trap.c:112：

  ### 内核态 trap 入口

  w_stvec((uint64)kernelvec);

  ### 返回用户态前改成用户 trap 入口

  w_stvec(trampoline_uservec);

  所以它解决的是：

  “下一次 trap 发生时，CPU 应该先跳到哪段入口代码？”

  ———

  ## 6. r_sstatus

  在 xv6-riscv/kernel/trap.c:42：

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  这表示检查当前 trap 之前是不是在用户态。

  在 xv6-riscv/kernel/trap.c:125：

  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP;
  x |= SSTATUS_SPIE;
  w_sstatus(x);

  这是返回用户态前修改状态。

  所以这组东西解决的是：

  “trap 之前 CPU 在哪一层、trap 返回后要恢复成什么状态？”

  ———

  ## 7. r_scause

  主要在 xv6-riscv/kernel/trap.c:54 这一段：

  if (r_scause() == 8) {
    // system call
  } else if ((which_dev = devintr()) != 0) {
    // interrupt
  } else if ((r_scause() == 15 || r_scause() == 13) && ...) {
    // page fault
  }

  所以它解决的是：

  “这次 trap 到底是系统调用、中断、还是 page fault？”

  ———

  ## 8. r_stval

  在 xv6-riscv/kernel/trap.c:72：

  vmfault(p->pagetable, r_stval(), ...)

  这里 stval 通常是 fault 的虚拟地址。

  所以它解决的是：

  “如果是地址错误/缺页，出问题的那个地址到底是多少？”

  ———

  ## 9. PX(level, va)

  出现在 xv6-riscv/kernel/vm.c:105：

  pte_t *pte = &pagetable[PX(level, va)];

  作用是从虚拟地址里取出：

  - level 2 索引
  - level 1 索引
  - level 0 索引

  所以它解决的是：

  “在三级页表里，这个虚拟地址每一层该走哪一个槽位？”

  ———

  ## 10. 你可以把这些函数按模块记

  ### 启动看这组

  - r_mhartid
  - w_mstatus
  - w_mepc
  - w_satp

  对应文件：

  - xv6-riscv/kernel/start.c:15

  ### trap 看这组

  - r_sstatus
  - w_stvec
  - r_scause
  - r_stval

  对应文件：

  - xv6-riscv/kernel/trap.c:37

  ### 页表看这组

  - MAKE_SATP
  - PX
  - PTE_*

  对应文件：

  - xv6-riscv/kernel/vm.c:20

  ———

  ## 一句话总结

  riscv.h 里的这些寄存器接口不是孤立的定义，它们分别落在 xv6 的三条主线上：

  - start.c 用它们完成启动和特权切换
  - trap.c 用它们完成 trap 识别和返回
  - vm.c 用它们完成页表寻址和地址空间切换

  如果你愿意，我下一条可以继续专门讲：

  sstatus、sepc、stvec 这三个寄存器在一次系统调用里的完整变化过程。


› 讲解一下trap.c


• trap.c 是 xv6 的 陷入/中断总控文件。

  一句话概括：

  它负责处理 CPU 从用户态或内核态“突然进入内核”的所有情况，包括系统调用、时钟中断、设备中断、异常、缺页。

  文件在 xv6-riscv/kernel/trap.c:1。

  ———

  ## 先给你总图

  这个文件主要有 6 个核心函数：

  - trapinit()
  - trapinithart()
  - usertrap()
  - prepare_return()
  - kerneltrap()
  - devintr()

  还有一个辅助函数：

  - clockintr()

  你可以把它们分成三组理解：

  1. 初始化 trap 环境
  2. 处理“从用户态进来的 trap”
  3. 处理“从内核态进来的 trap”

  ———

  # 1. trap 是什么

  trap 可以先粗暴理解成：

  CPU 正在正常执行，突然因为某件事跳进内核。

  这些“某件事”包括：

  - 用户程序执行 ecall，发起系统调用
  - 时钟到了，产生 timer interrupt
  - UART/磁盘设备发中断
  - 访问非法地址，产生 page fault
  - 执行异常指令

  所以 trap.c 的任务就是回答两个问题：

  1. 这次为什么进内核？
  2. 处理完之后怎么回去？

  ———

  # 2. trapinit()

  在 xv6-riscv/kernel/trap.c:20

  void
  trapinit(void)
  {
    initlock(&tickslock, "time");
  }

  它很简单，只初始化：

  - tickslock
  - 用来保护全局时钟计数 ticks

  说明：

  - 真正 trap 入口地址不是在这里设的
  - 这里只做和 trap 子系统相关的数据初始化

  ———

  # 3. trapinithart()

  在 xv6-riscv/kernel/trap.c:27

  void
  trapinithart(void)
  {
    w_stvec((uint64)kernelvec);
  }

  作用：

  给当前 CPU 设置 trap 入口地址。

  这里把 stvec 设成 kernelvec，意思是：

  - 当前 CPU 如果在内核态发生 trap
  - 先跳到 kernelvec.S 那段汇编入口

  注意这是 每个 hart 都要做一次 的，所以叫 trapinithart()。

  ———

  # 4. usertrap()：用户态进入内核的主入口

  在 xv6-riscv/kernel/trap.c:37

  这个函数最重要。

  它处理的是：

  用户进程在用户态运行时发生 trap，然后进入内核。

  ———

  ## 4.1 先检查 trap 来源

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  意思是：

  - usertrap() 预期 trap 来自用户态
  - 如果 SPP != 0，说明 trap 前其实在 supervisor mode
  - 那就错了，直接 panic

  ———

  ## 4.2 先把 trap 入口切回 kernelvec

  w_stvec((uint64)kernelvec);

  为什么？

  因为现在已经进内核了。
  如果内核执行过程中再次发生 trap，应该交给 kerneltrap()，而不是又走用户入口。

  所以这句是在说：

  “接下来如果内核里再 trap，就走内核 trap 路径。”

  ———

  ## 4.3 保存用户 PC

  p->trapframe->epc = r_sepc();

  这是关键动作。

  因为用户态被打断了，必须记住它原来执行到哪。
  以后返回用户态时，要从这里继续。

  ———

  ## 4.4 分三类处理

  ### 第一类：系统调用

  if (r_scause() == 8) {

  scause == 8 表示用户态 ecall。

  处理流程：

  1. 如果进程已经被 kill，就退出
  2. epc += 4
     因为 sepc 指向的是 ecall 指令本身，返回时要跳过它

  3. intr_on()
     处理 syscall 前重新开中断

  4. syscall()
     真正进入系统调用分发

  ———

  ### 第二类：设备中断

  } else if ((which_dev = devintr()) != 0) {

  这里调用 devintr() 识别：

  - 是不是 UART 中断
  - 是不是 virtio 磁盘中断
  - 是不是 timer interrupt

  如果是，就处理掉。

  ———

  ### 第三类：缺页异常

  } else if ((r_scause() == 15 || r_scause() == 13) &&
             vmfault(p->pagetable, r_stval(), (r_scause() == 13) ? 1 : 0) != 0) {

  这说明你这个版本支持 lazy allocation。

  含义是：

  - scause == 13：load page fault
  - scause == 15：store page fault

  如果 fault 地址属于合法地址空间，就调用 vmfault() 现场补一页。

  这和标准教材版 xv6 不完全一样，是你这个版本的重要改动。

  ———

  ### 否则：未知异常

  printf("usertrap(): unexpected scause ...");
  setkilled(p);

  说明这是无法恢复的异常，记日志并杀掉进程。

  ———

  ## 4.5 trap 处理完后的统一收尾

  ### 如果进程被 kill，就退出

  if (killed(p))
    kexit(-1);

  ### 如果是时钟中断，让出 CPU

  if (which_dev == 2)
    yield();

  为什么？

  因为 timer interrupt 是调度时机。
  这就是时间片轮转的关键点之一。

  ### 准备返回用户态

  prepare_return();
  uint64 satp = MAKE_SATP(p->pagetable);
  return satp;

  prepare_return() 负责设置各种寄存器和 trapframe。
  最后返回用户页表的 satp，供 trampoline 使用。

  ———

  # 5. prepare_return()：回用户态前的准备

  在 xv6-riscv/kernel/trap.c:100

  这是 usertrap() 的配套函数。

  作用是：

  把 CPU 和 trapframe 调整到“马上可以回用户态”的状态。

  ———

  ## 5.1 先关中断

  intr_off();

  因为接下来要改 trap 入口和状态，不能被打断。

  ———

  ## 5.2 把下一次用户 trap 的入口设成 uservec

  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  这句很重要。

  意思是：

  - 以后如果用户态再次 trap
  - 不应该先跳 kernelvec
  - 而应该先跳 trampoline 页里的 uservec

  这就是 TRAMPOLINE 真正发挥作用的地方。

  ———

  ## 5.3 填 trapframe 的 kernel_* 字段

  p->trapframe->kernel_satp = r_satp();
  p->trapframe->kernel_sp = p->kstack + PGSIZE;
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();

  这些是为了下次用户态 trap 回来时，uservec 能知道：

  - 内核页表是谁
  - 当前进程内核栈在哪
  - 应该跳到哪个 C 函数处理 trap
  - 当前 hart id 是多少

  ———

  ## 5.4 设置 sstatus

  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP;
  x |= SSTATUS_SPIE;
  w_sstatus(x);

  作用：

  - 清 SPP
    表示 sret 后要回到用户态，不是 supervisor

  - 置 SPIE
    表示回到用户态后中断打开

  ———

  ## 5.5 设置用户返回地址

  w_sepc(p->trapframe->epc);

  表示回用户态后，从被打断的用户 PC 继续执行。

  ———

  # 6. kerneltrap()：内核态 trap 主入口

  在 xv6-riscv/kernel/trap.c:137

  它处理的是：

  内核代码自己运行时发生 trap。

  这和 usertrap() 不同，因为这时已经在内核态。

  ———

  ## 6.1 先保存现场

  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  因为后面可能发生调度切换，要先把这些寄存器值存起来。

  ———

  ## 6.2 做一致性检查

  if ((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  说明：

  - kerneltrap() 必须来自 supervisor mode
  - 进入时中断应该是关闭的

  否则都属于内核逻辑错误。

  ———

  ## 6.3 只接受“已知设备中断”

  if ((which_dev = devintr()) == 0) {
    printf(...);
    panic("kerneltrap");
  }

  和 usertrap() 不同，这里如果不是设备中断，基本就视为严重错误。

  因为内核态异常通常意味着内核 bug。

  ———

  ## 6.4 timer interrupt 也可能触发调度

  if (which_dev == 2 && myproc() != 0)
    yield();

  如果当前 CPU 正在为某个进程服务，而且时钟到了，就让出 CPU。

  ———

  ## 6.5 恢复 trap 相关寄存器

  w_sepc(sepc);
  w_sstatus(sstatus);

  因为 yield() 等过程中可能动过这些寄存器，所以要恢复。

  ———

  # 7. clockintr()

  在 xv6-riscv/kernel/trap.c:167

  作用：

  1. 更新全局 ticks
  2. 唤醒睡在 ticks 上的进程
  3. 设定下一次 timer interrupt

  w_stimecmp(r_time() + 1000000);

  这句很关键，表示 timer interrupt 不是自动周期来的，必须每次手动预约下一次。

  ———

  # 8. devintr()：识别并处理设备/时钟中断

  在 xv6-riscv/kernel/trap.c:187

  它是 trap 分流器。

  ———

  ## 8.1 外部中断

  if (scause == 0x8000000000000009L) {

  表示 supervisor external interrupt，通过 PLIC 进来。

  接着：

  int irq = plic_claim();

  问 PLIC：到底哪个设备中断了？

  ### UART

  if (irq == UART0_IRQ)
    uartintr();

  ### virtio 磁盘

  else if (irq == VIRTIO0_IRQ)
    virtio_disk_intr();

  处理完后：

  plic_complete(irq);

  告诉 PLIC：这个中断处理完了，可以继续接收下一次。

  返回值是：

  return 1;

  表示“普通设备中断”。

  ———

  ## 8.2 时钟中断

  } else if (scause == 0x8000000000000005L) {

  这是 supervisor timer interrupt。

  处理：

  clockintr();
  return 2;

  所以 which_dev == 2 在上层就被当成“时钟中断”。

  ———

  ## 8.3 其他情况

  return 0;

  表示不是已知设备中断。

  ———

  # 9. 把整个 trap 控制流串起来

  ———

  ## 用户态系统调用

  user program
  → ecall
  → uservec in trampoline
  → usertrap()
  → syscall()
  → prepare_return()
  → userret
  → 回用户态

  ———

  ## 用户态 page fault

  user program
  → fault
  → usertrap()
  → r_scause()/r_stval()
  → vmfault()
  → prepare_return()
  → 回用户态

  ———

  ## 时钟中断

  timer interrupt
  → usertrap() 或 kerneltrap()
  → devintr()
  → clockintr()
  → 可能 yield()

  ———

  ## UART / 磁盘中断

  device interrupt
  → devintr()
  → uartintr() / virtio_disk_intr()

  ———

  # 10. 你现在最该抓住的 5 个点

  1. usertrap() 处理“从用户态进来的 trap”
  2. kerneltrap() 处理“从内核态进来的 trap”
  3. devintr() 负责区分时钟中断和设备中断
  4. prepare_return() 负责回用户态前的寄存器和 trapframe 准备
  5. 你这个版本多了一条 vmfault() 路线，支持 lazy allocation

  ———

  # 11. 一句话总结

  trap.c 是 xv6 的 trap 调度中心：
  它负责接住用户态和内核态的所有陷入事件，判断原因，分发给系统调用/设备中断/缺页处理，并在处理后把 CPU 正确送回原来的执行流。

  如果你愿意，我下一条可以继续讲最关键的一条链：

  一次系统调用从 ecall 到 usertrap() 再到返回用户态的完整执行过程。


bio.c 是 xv6 文件系统里的 块缓存层（buffer cache）。它的作用不是“实现文件读写语义”，而是负责：

  - 把磁盘块缓存到内存里
  - 给上层提供统一的 bread/bwrite/brelse 接口
  - 作为多个进程访问同一磁盘块时的同步点
  - 给日志层和 inode 层提供可复用的块对象

  核心文件是 kernel/bio.c:1，配套结构在 kernel/buf.h:1。

  1. 它在文件系统里的位置

  可以把文件系统分层理解成：

  - 最底层：磁盘驱动 virtio_disk.c
  - 往上一层：bio.c 块缓存
  - 再往上：log.c 日志
  - 再往上：fs.c 的 inode、目录、路径
  - 最上层：sysfile.c 的系统调用

  也就是说，fs.c 不直接碰 VirtIO 驱动，而是通过 bio.c 读写块。

  2. 它缓存的是什么

  缓存单位是“磁盘块”，不是“文件”。

  每个缓存块对应一个 struct buf，里面通常有这些信息：

  - 这是哪个设备、哪个块号
  - 数据是否有效
  - 是否正在被引用
  - 一把 sleep lock
  - 这一块的 1024 字节数据

  bio.c 维护了一个全局缓存 bcache，见 kernel/bio.c:25。其中有：

  - 一把自旋锁 bcache.lock
  - 固定数量的缓存块数组 buf[NBUF]
  - 一个双向链表，按“最近使用”顺序组织

  3. 初始化做了什么

  binit() 负责初始化整个块缓存，见 kernel/bio.c:35。

  它做两件事：

  - 初始化全局锁
  - 把所有 buf 串成一个双向链表

  这个链表的约定是：

  - head.next 是最近使用的
  - head.prev 是最久未使用的

  所以这是一个简化版 LRU 缓存。

  4. 最核心的函数：bget()

  bget(dev, blockno) 是内部核心，见 kernel/bio.c:57。

  它的逻辑是：

  1. 先拿 bcache.lock
  2. 在缓存链表里找目标块是否已经存在
  3. 如果存在：
      - refcnt++
      - 释放全局锁
      - 获取这个块自己的 sleep lock
      - 返回它

  4. 如果不存在：
      - 从链表尾部找一个 refcnt==0 的空闲块
      - 把它“回收”成新目标块
      - 设置 dev/blockno/valid/refcnt
      - 获取这个块的 sleep lock
      - 返回它

  5. 如果连空闲块都没有，就 panic

  这里有两个很重要的点：

  - refcnt 表示“当前有多少人持有这个缓存块”
  - sleep lock 保证“同一时刻只有一个执行流能操作这个块内容”

  5. bread()：读块入口

  bread(dev, blockno) 是上层最常用的接口，见 kernel/bio.c:91。

  它先调用 bget() 拿到一个“已锁定”的 buf。然后：

  - 如果 valid == 0，说明缓存里还没有真实数据
  - 就调用 virtio_disk_rw(b, 0) 从磁盘读入
  - 然后把 valid = 1

  所以 bread() 返回时，上层拿到的是：

  - 已经加锁
  - 数据已可用
  - 可以安全读写 b->data

  6. bwrite()：写块到磁盘

  bwrite(b) 见 kernel/bio.c:104。

  它要求调用者必须已经持有这个块的 sleep lock，否则 panic。然后直接调用：

  - virtio_disk_rw(b, 1)

  也就是把当前缓存块内容同步写回磁盘。

  不过在文件系统正常修改路径里，很多时候并不直接调用 bwrite()，而是调用日志层的 log_write()。这是因为 xv6 要保证多步更新的崩溃一致性。

  7. brelse()：释放块

  brelse(b) 见 kernel/bio.c:113。

  它的逻辑是：

  1. 检查当前确实持有块锁
  2. 释放这个块的 sleep lock
  3. 拿全局 bcache.lock
  4. refcnt--
  5. 如果 refcnt == 0，把这个块移动到链表头部，表示“刚使用过”
  6. 释放全局锁

  这里有个容易看错的点：

  - xv6 不是在“拿到块时”更新 LRU
  - 而是在“最后一个使用者释放块时”把它移到 MRU 位置

  这样链表尾部始终更接近“最久没被用且没人占着”的块，方便回收。

  8. bpin() / bunpin() 是干什么的

  这两个函数见 kernel/bio.c:138。

  它们只改 refcnt，不拿块的 sleep lock。

  用途是给日志层用：

  - 当某个块被 log_write() 记录进当前事务时，日志层会 bpin(b)
  - 这样即使调用者 later brelse()，这个块也不会被缓存系统回收
  - 等事务提交完成后，再 bunpin(b)

  所以它们是在实现：

  - “这个块虽然暂时没人直接在用，但事务还依赖它，不能被替换掉”

  9. 为什么要同时有自旋锁和睡眠锁

  bio.c 用了两级锁：

  - bcache.lock：自旋锁，保护全局缓存结构、链表、refcnt
  - b->lock：sleep lock，保护某个具体块的数据内容

  原因是：

  - 全局链表操作很短，适合自旋锁
  - 块内容访问可能伴随磁盘 I/O，适合 sleep lock

  这个设计避免了：

  - 用一把大锁把所有块 I/O 串行化
  - 或者反过来完全没有同步导致同一块被并发修改

  10. 它和 fs.c / log.c 的关系

  和 fs.c 的关系：

  - fs.c 里的 balloc()、bfree()、bmap()、readi()、writei() 都大量调用 bread() / brelse()
  - 所以 inode 层读写数据块、位图块、间接块时，都是通过 bio.c

  和 log.c 的关系：

  - log.c 不自己管理块缓存
  - 它依赖 bio.c 提供块对象
  - 再在此基础上做事务日志、commit、recovery

  所以可以说：

  - bio.c 解决“块怎么安全高效地在内存里复用”
  - log.c 解决“多个块修改怎么原子提交”

  11. 这个文件的设计特点

  bio.c 的设计很典型，也很教学化：

  优点：

  - 简单清晰
  - 明确区分缓存管理和块内容加锁
  - 能支撑上层 inode/日志逻辑
  - 有 LRU 近似替换策略

  限制：

  - 缓存块数固定，受 NBUF 限制
  - 全局只有一把 bcache.lock，扩展性一般
  - 不是更复杂的哈希桶缓存
  - 替换策略比较粗糙
  - 没有预读、写回合并之类优化

  12. 一句话概括

  你可以把 bio.c 概括成：

  > bio.c 是 xv6 文件系统的块缓存与块同步层，负责把磁盘块缓存到内存中，并通过 bread/bwrite/brelse 提供统一块访问接口；它使用“全局缓存链表 + 每块独立锁”的方式，实现缓存复用、LRU 式替
  > 换以及同一磁盘块的并发访问同步，是上层日志、inode、目录和路径解析机制的基础。

  如果你要，我下一步可以继续按同样风格解释 log.c，或者把 bio.c 这一段整理成适合写进 Maple/Done.md 的版本。





› 解释log.c


• log.c 是 xv6 文件系统里的 日志层（write-ahead logging / redo log）。它的目标很明确：

  保证文件系统一次系统调用涉及的多块更新，要么整体生效，要么崩溃后能恢复，不会把磁盘留在“改了一半”的不一致状态。

  核心文件是 kernel/log.c:1。

  ———

  ## 1. 它为什么存在

  文件系统操作通常不是改一个块就结束。

  比如创建一个文件，往往至少要改：

  - inode 位图或 inode 表
  - 父目录的数据块
  - 可能还有数据位图
  - 可能还有日志头本身

  如果系统在这些步骤中间崩溃，就可能出现：

  - 目录里有名字，但 inode 没初始化好
  - inode 分配了，但目录里没有它
  - 数据块标记已占用，但文件没引用它

  这就是“磁盘不一致”。

  log.c 的作用就是避免这个问题。

  ———

  ## 2. 它的基本思路

  xv6 采用的是 物理 redo log。

  意思是：

  - 不是记录“做了什么逻辑操作”
  - 而是直接记录“哪些磁盘块修改后的内容应该写到哪里”

  基本流程是：

  1. 文件系统系统调用开始时调用 begin_op()
  2. 修改块时，不直接立刻提交最终结果，而是通过 log_write() 把这个块记入当前事务
  3. 当所有正在进行的文件系统操作都结束后，统一 commit
  4. commit 时先把修改过的块写到日志区
  5. 再把日志头写盘，这一步表示“事务正式提交”
  6. 然后把日志区内容拷贝回这些块的真实位置
  7. 最后清空日志

  所以核心原则是：

  先写日志，再写正式位置。

  ———

  ## 3. 磁盘上的日志长什么样

  log.c 里的注释写得很直接，日志区格式是：

  - 一个 header block
  - 后面跟若干个日志数据块

  见 kernel/log.c:24。

  header 结构是 struct logheader，见 kernel/log.c:35，里面有：

  - n：这次事务一共记录了多少个块
  - block[]：这些块最终应该写回到哪些“真实块号”

  所以日志区本质上是：

  - header 记录“本次事务涉及哪些目标块”
  - 后续每个日志块保存这些目标块修改后的完整内容

  ———

  ## 4. 内存里的全局日志状态

  struct log 定义在 kernel/log.c:40，主要字段有：

  - start：日志区起始块号
  - dev：设备号
  - outstanding：当前有多少文件系统系统调用正在执行
  - committing：当前是否正在提交
  - lh：内存中的日志头

  这说明 xv6 的日志是全局单实例的，不是每个进程一份。

  ———

  ## 5. initlog() 和恢复机制

  initlog() 在文件系统初始化时调用，见 kernel/log.c:53。

  它会：

  1. 初始化锁
  2. 记录日志区起点和设备号
  3. 调用 recover_from_log()

  recover_from_log() 见 kernel/log.c:117，流程是：

  1. 先读日志头 read_head()
  2. 如果日志头里 n > 0，说明上次崩溃前有一个已提交但还没完全安装回原位的事务
  3. 调用 install_trans(1) 把日志块拷贝回真实块
  4. 把 log.lh.n = 0
  5. 再把空日志头写回磁盘

  所以 xv6 的恢复逻辑非常简单：

  - 如果日志头表明有已提交事务，就重放
  - 重放完清空日志

  这就是 redo log 的典型做法。

  ———

  ## 6. begin_op() / end_op() 是做什么的

  这两个函数是给整个文件系统系统调用用的边界标记。

  ### begin_op()

  见 kernel/log.c:126

  它表示：

  - “我要开始做一次文件系统操作了”

  主要做三件事：

  1. 如果当前正在 commit，就睡眠等待
  2. 如果日志空间可能不够这次操作使用，也等待
  3. 否则 outstanding++，允许这次操作进入

  这里有个很关键的设计：

  begin_op() 会为“所有并发中的文件系统系统调用”预留日志空间，避免大家一起改块把日志挤爆。

  ### end_op()

  见 kernel/log.c:145

  它表示：

  - “这次文件系统操作结束了”

  逻辑是：

  1. outstanding--
  2. 如果这不是最后一个活动操作，就只是唤醒可能在等日志空间的人
  3. 如果这是最后一个活动操作，就触发一次真正的 commit()

  所以 xv6 的策略不是“每个系统调用单独马上提交”，而是：

  只要还有文件系统操作在执行，就先攒着；等最后一个结束时统一提交。

  ———

  ## 7. log_write() 到底做了什么

  log_write() 见 kernel/log.c:215。

  它并不直接把块写到磁盘日志区，而是：

  1. 检查当前必须在事务里，否则 panic
  2. 看这个块号是否已经在本次事务的 lh.block[] 里
  3. 如果已经有了，就不重复加
  4. 如果还没有，就把块号加入日志头数组
  5. 同时 bpin(b)，防止这个缓存块在 commit 前被回收

  这有两个重点：

  ### 1. 它是“登记修改块”，不是“立即提交”

  真正写日志是在 commit() 时做。

  ### 2. 它有 log absorption

  如果同一个块在一次事务里被改了很多次，只记录一次块号即可。最终提交时写的是“最后状态”。

  这能减少日志占用。

  ———

  ## 8. commit() 是怎么完成一次事务的

  commit() 见 kernel/log.c:194。

  它做四步：

  ### 第一步：write_log()

  见 kernel/log.c:178

  把当前事务涉及的每个缓存块内容，复制到日志区的数据块里。

  这时只是“日志数据块写好了”，还不算事务正式提交。

  ### 第二步：write_head()

  见 kernel/log.c:100

  把内存中的日志头写到磁盘日志头块。

  这一步是真正的 commit 点。

  为什么？
  因为只要 header 落盘，系统重启后就知道：

  - 有一笔完整事务存在
  - 应该把哪些日志块安装到哪些真实块

  ### 第三步：install_trans(0)

  见 kernel/log.c:65

  把日志区里的块一个个拷贝到真实目标块。

  这一步才让文件系统“正式变成新状态”。

  ### 第四步：清空日志头

  log.lh.n = 0 后再 write_head() 一次，把日志头清空。

  这表示：

  - 本次事务已经彻底完成
  - 重启后不需要再重放

  ———

  ## 9. 为什么“先写日志头”这么关键

  这是整个 log.c 最核心的地方。

  如果系统在 write_log() 之后、write_head() 之前崩溃：

  - 日志数据块可能写了一半
  - 但 header 还没写
  - 恢复时会认为“没有有效事务”
  - 所以不会重放

  这没关系，因为真实 home block 还没被改，磁盘仍是一致的旧状态。

  如果系统在 write_head() 之后、install_trans() 之前崩溃：

  - header 已写盘
  - 恢复时会看到一个完整事务
  - 重启时调用 recover_from_log() 重放即可

  所以 header 就像“提交标志”。

  ———

  ## 10. install_trans() 在恢复和平时提交里都复用

  install_trans() 见 kernel/log.c:67。

  它做的事很直接：

  - 读日志块
  - 读目标块
  - 把日志块内容复制到目标块
  - bwrite() 写回目标块

  这个函数既用于：

  - 正常 commit 之后把更新安装回 home block
  - 崩溃恢复时把已提交事务重放到 home block

  所以恢复逻辑和正常提交使用的是同一套“拷贝回原位置”的代码。

  ———

  ## 11. 它和 bio.c 的关系

  log.c 自己不管理块缓存，它依赖 bio.c。

  具体来说：

  - bread()/bwrite()/brelse() 都来自 bio.c
  - log_write() 里调用 bpin() / bunpin() 也是 bio.c 提供的

  关系可以这样理解：

  - bio.c 负责“块在内存里怎么缓存和同步”
  - log.c 负责“多个块修改怎么作为一个事务提交”

  ———

  ## 12. 它的设计特点和限制

  优点：

  - 实现简单
  - 能保证基本崩溃一致性
  - 恢复逻辑直接
  - 非常适合教学系统

  限制：

  - 只有一个全局日志
  - 事务大小受 LOGBLOCKS 限制
  - 不是高性能日志文件系统
  - 不支持更复杂的并发事务隔离
  - 不是细粒度 journal，只是块级 redo log

  所以它不是 ext4 那种工业级 journaling，但对 xv6 足够。

  ———

  ## 13. 一句话总结

  你可以把 log.c 概括成：

  > log.c 实现了 xv6 文件系统的写前日志机制。它通过 begin_op/end_op 管理文件系统事务边界，通过 log_write 记录本次事务修改过的磁盘块，并在 commit 时先将这些块写入日志区、再写日志头完
  > 成提交、最后安装回真实位置；系统启动时若发现日志头中存在已提交事务，则自动重放，从而保证文件系统在崩溃后的磁盘一致性。

  如果你要，我下一步可以继续解释 fs.c，或者把 bio.c + log.c 合并整理成适合写进 Maple/Done.md 的文档版本。


 解读fs.c


• 我按 fs.c 的内部结构来拆，不泛讲“文件系统原理”，而是直接对应这份 xv6 代码：初始化、块分配、inode 生命周期、数据块映射、读写、目录、路径解析。

• Explored
  └ Read fs.c

──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

• fs.c 是 xv6 文件系统的核心语义层。如果说：

  - bio.c 解决“磁盘块怎么缓存”
  - log.c 解决“多块修改怎么事务提交”

  那 fs.c 解决的就是：

  如何把这些块组织成真正的“文件、目录、路径和读写”

  文件开头自己就写了五层模型，见 kernel/fs.c:1：

  - blocks：原始磁盘块分配
  - log：日志恢复
  - files：inode、元数据、读写
  - directories：目录项
  - names：路径名解析

  可以把 fs.c 理解成“块设备之上的 Unix 文件系统本体”。

  ———

  ## 1. 总体职责

  fs.c 主要负责这几件事：

  - 读取并初始化文件系统 superblock
  - 分配/释放磁盘块
  - 分配/缓存/回收 inode
  - 把文件逻辑块号映射到实际磁盘块号
  - 从 inode 读写文件内容
  - 把目录当作特殊文件来管理
  - 逐层解析路径名

  也就是说，sysfile.c 里的 open/unlink/mkdir/chdir 虽然是系统调用入口，但真正的文件系统能力几乎都落在 fs.c。

  ———

  ## 2. superblock 与文件系统初始化

  ### sb

  全局 struct superblock sb; 定义在 kernel/fs.c:27。

  它保存整个文件系统布局：

  - 总块数
  - inode 区起始位置
  - 日志区位置
  - 位图区位置

  这些字段的结构定义在 fs.h。

  ### readsb()

  kernel/fs.c:30 通过 bread(dev, 1) 读取磁盘第 1 块，把 superblock 拷到内存。

  ### fsinit()

  kernel/fs.c:41 是文件系统启动初始化入口，做三件事：

  1. 读入 superblock
  2. 检查 magic 是否正确
  3. 初始化日志 initlog(dev, &sb)
  4. 调用 ireclaim(dev) 回收 orphaned inode

  所以 fsinit() 不是“建文件系统”，而是“把磁盘上已有文件系统接到内核里并恢复状态”。

  ———

  ## 3. 磁盘块分配层

  这一层管理的是：

  哪些磁盘块空闲，哪些已被文件或元数据占用。

  ### bzero()

  kernel/fs.c:52 把某个磁盘块清零。
  它先 bread() 读块，再 memset()，然后 log_write() 记入日志。

  这说明即使“清零一个新块”也是事务化的。

  ### balloc()

  kernel/fs.c:68 从位图中找一个空闲块：

  1. 按 bitmap block 扫描
  2. 找到某一位是 0，表示该块空闲
  3. 把这一位设成 1
  4. 通过 log_write() 记录位图块修改
  5. 调用 bzero() 清空新块
  6. 返回这个块号

  所以 balloc() 同时完成：

  - 位图分配
  - 新块清零

  ### bfree()

  kernel/fs.c:94 与之相反，把某块在位图里标成空闲。
  如果它本来就是空闲的，会 panic，防止 double free。

  ———

  ## 4. inode 层：文件对象的核心表示

  这一层是 fs.c 的中心。

  ### inode 的意义

  注释从 kernel/fs.c:109 开始写得很清楚：

  - inode 表示一个“无名文件对象”
  - 名字不在 inode 里，名字在目录项里
  - inode 保存的是：
      - 类型
      - 大小
      - 链接数
      - 数据块地址

  ### 内存中的 inode 表 itable

  定义在 kernel/fs.c:178。

  它是一个全局数组缓存，保存当前“活跃”的 inode。
  内存 inode 比磁盘 inode 多了：

  - ref：被多少内核对象引用
  - valid：磁盘内容是否已读入内存
  - lock：sleep lock，保护 inode 内容

  ### iinit()

  kernel/fs.c:183 初始化 inode 表及每个 inode 的 sleep lock。

  ———

  ## 5. inode 分配、查找、引用与回收

  ### ialloc()

  kernel/fs.c:200 在磁盘 inode 区里线性扫描，找一个 type == 0 的空闲 inode：

  1. 找到空闲 dinode
  2. 清零
  3. 设置 type
  4. log_write() 记日志
  5. 返回对应的内存 inode iget(dev, inum)

  所以 ialloc() 分配的是磁盘 inode，然后再拿到它对应的内存 inode 引用。

  ### iupdate()

  kernel/fs.c:227 把内存 inode 的关键字段刷回磁盘：

  - type
  - major/minor
  - nlink
  - size
  - addrs

  只要你修改了 inode 的持久化字段，就要调用 iupdate()。

  ### iget()

  kernel/fs.c:248 只做一件事：

  在内存 inode 表中找到或创建某个 (dev, inum) 对应的内存 inode 项，并增加 ref。

  注意它：

  - 不加 inode 内容锁
  - 不读磁盘内容
  - 只是拿“引用”

  这是 xv6 很重要的设计点：
  iget() 和 ilock() 分离。

  ### idup()

  kernel/fs.c:283 就是 ref++，用于复制 inode 引用，比如 cwd 继承。

  ### ilock()

  kernel/fs.c:292 负责：

  1. 获得 inode 的 sleep lock
  2. 如果 valid == 0，从磁盘读 dinode 到内存 inode

  所以 iget() 是“拿到一个 inode 句柄”，ilock() 才是“真正准备读写它”。

  ### iunlock()

  kernel/fs.c:321 释放 inode sleep lock。

  ### iput()

  kernel/fs.c:339 是 inode 生命周期里最关键的回收函数。

  如果满足：

  - ref == 1
  - valid == 1
  - nlink == 0

  说明：

  - 这是内存中最后一个引用
  - 磁盘上也没有目录项再指向它

  那它就会：

  1. itrunc(ip) 释放内容块
  2. ip->type = 0
  3. iupdate(ip) 把 inode 标为空闲
  4. valid = 0

  最后再把 ref--。

  也就是说：

  真正删除一个文件，不是在 unlink() 时立刻完成，而是在 iput() 发现它既没名字也没人引用时完成。

  ### iunlockput()

  kernel/fs.c:366 是常用组合：先解锁，再 iput()。

  ### ireclaim()

  kernel/fs.c:374 用于启动恢复 orphaned inode。
  如果磁盘上有 type != 0 && nlink == 0 的 inode，说明上次崩溃前它本该被回收但没做完，这里会触发回收闭环。

  ———

  ## 6. 文件内容层：块映射与截断

  ### bmap()

  kernel/fs.c:407 是整个文件内容访问的核心。

  输入：

  - inode ip
  - 文件中的逻辑块号 bn

  输出：

  - 对应的真实磁盘块号

  逻辑是：

  1. 如果 bn < NDIRECT，走直接块数组 ip->addrs[bn]
  2. 否则减去 NDIRECT，走一级间接块
  3. 如果对应块不存在，就调用 balloc() 分配

  所以 bmap() 不是单纯查表，它还会在写路径中按需分配新块。

  ### itrunc()

  kernel/fs.c:450 负责释放一个 inode 的全部内容块：

  - 释放所有直接块
  - 若有间接块，则先释放间接块里指向的数据块，再释放间接块自身
  - 把 size = 0
  - iupdate(ip)

  它用于：

  - O_TRUNC
  - 文件真正删除时的回收

  ———

  ## 7. 读写层：readi() 和 writei()

  ### stati()

  kernel/fs.c:481 把 inode 元数据拷到 struct stat。

  ### readi()

  kernel/fs.c:496 负责从 inode 内容里读数据。

  流程：

  1. 检查偏移是否越界
  2. 把 n 截断到文件末尾以内
  3. 循环：
      - 通过 bmap() 找块
      - bread() 读块
      - 算出块内偏移与本次要拷贝的字节数
      - either_copyout() 拷给用户或内核地址

  4. 返回实际读取字节数

  这里 user_dst 参数表示目标地址是否是用户空间地址。

  ### writei()

  kernel/fs.c:530 与之对称，负责写数据。

  流程：

  1. 检查偏移与最大文件长度
  2. 循环：
      - 通过 bmap() 找块；必要时分配新块
      - bread() 读块
      - either_copyin() 从用户或内核地址拷入块缓存
      - log_write() 标记该缓存块被修改

  3. 如果写到了原文件末尾之后，更新 ip->size
  4. iupdate(ip) 写回 inode 元数据
  5. 返回已写字节数

  关键点是：

  - writei() 不直接 bwrite()，而是 log_write()
  - 说明文件内容更新受日志保护

  ———

  ## 8. 目录层：目录本质上是特殊文件

  从 kernel/fs.c:574 开始进入目录逻辑。

  ### namecmp()

  kernel/fs.c:576 用 DIRSIZ 固定长度比较目录项名字。

  ### dirlookup()

  kernel/fs.c:584 在目录 inode 中线性扫描目录项：

  1. 每次用 readi() 读一个 dirent
  2. 跳过空项 de.inum == 0
  3. 名字匹配就返回对应 inode iget(dp->dev, inum)

  所以目录并不是 hash 表或树结构，而就是一个普通文件，内容是一串 dirent。

  ### dirlink()

  kernel/fs.c:613 往目录里插入新项：

  1. 先检查名字是否已存在
  2. 再找一个空目录项槽位
  3. 写入 (name, inum)

  这就是创建文件/目录时“把名字挂到父目录”的核心。

  ———

  ## 9. 名字层：路径解析

  从 kernel/fs.c:641 开始。

  ### skipelem()

  kernel/fs.c:655 每次从路径字符串里取出下一段元素，比如：

  - /a/b/c -> 先取 a
  - 再取 b
  - 再取 c

  它会跳过多余的 /。

  ### namex()

  kernel/fs.c:684 是路径解析核心。

  逻辑：

  1. 如果路径以 / 开头，从根目录 ROOTINO 开始
  2. 否则从当前进程 cwd 开始
  3. 循环调用 skipelem() 取路径段
  4. 每一层都：
      - ilock(ip)
      - 检查它必须是目录
      - 调用 dirlookup() 找下一层

  5. 若 nameiparent 为真，则在最后一层前停下，返回父目录

  这个函数支撑了：

  - namei(path)：返回路径最终对象 kernel/fs.c:719
  - nameiparent(path, name)：返回父目录和最后一段名字 kernel/fs.c:726

  所以几乎所有文件系统系统调用：

  - open
  - mkdir
  - unlink
  - link
  - chdir

  最终都依赖 namei/nameiparent。

  ———

  ## 10. fs.c 和其它文件的边界

  可以这样分工理解：

  ### fs.c

  负责“文件系统对象语义”：

  - inode
  - 数据块
  - 目录
  - 路径

  ### file.c

  负责“打开文件对象语义”：

  - struct file
  - 引用计数
  - 当前偏移 off
  - inode/pipe/device 分流

  ### sysfile.c

  负责“系统调用入口与参数检查”：

  - open
  - read
  - write
  - close
  - mkdir
  - unlink
  - link
  - chdir

  所以 fs.c 是最“像文件系统本体”的文件。

  ———

  ## 11. 看 fs.c 时最该抓住的主线

  如果你要真正吃透它，优先抓这三条线：

  ### 1. inode 生命周期

  ialloc -> iget -> ilock -> iupdate -> iput -> itrunc

  ### 2. 文件内容访问

  readi/writei -> bmap -> balloc/bfree

  ### 3. 路径与目录

  namex -> dirlookup/dirlink

  只要这三条线通了，fs.c 基本就通了。

  ———

  ## 12. 一句话概括

  你可以把 fs.c 总结成：

  > fs.c 是 xv6 文件系统的核心语义实现，负责管理 superblock、磁盘块分配、inode 生命周期、文件数据块映射、目录项维护和路径解析。它建立了从“磁盘块”到“文件/目录/路径”这一层抽象，是
  > sysfile.c 系统调用层与底层 bio.c/log.c 之间的关键桥梁。

  如果你愿意，我下一步可以继续做两种事里的任意一种：

  1. 按“函数调用链”方式，拿 open() / unlink() / mkdir() 各走一遍 fs.c。
  2. 直接把这份 fs.c 解读压缩成适合写进 Maple/Done.md 的文档版。


› inode 和 dinode的对比


• dinode 和 inode 的关系可以直接概括成一句话：

  dinode 是磁盘上的 inode 格式，inode 是内存中的 inode 对象。

  它们表示的是“同一个文件对象”的两种存在形态，但职责不一样。

  ———

  ## 1. dinode 是什么

  dinode 定义在 kernel/fs.h:30：

  struct dinode {
    short type;
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
  };

  它的特点是：

  - 存在于磁盘 inode 区
  - 是文件系统镜像的一部分
  - 只保存需要持久化的信息
  - 不包含锁、引用计数这类运行时信息

  也就是说，dinode 记录的是一个文件在磁盘上的“正式档案”。

  它保存的内容有：

  - 文件类型 type
  - 设备号 major/minor
  - 链接数 nlink
  - 文件大小 size
  - 数据块地址 addrs[]

  这些都是断电重启后必须还能保留下来的信息。

  ———

  ## 2. inode 是什么

  inode 定义在 kernel/file.h:16：

  struct inode {
    uint dev;
    uint inum;
    int ref;
    struct sleeplock lock;
    int valid;

    short type;
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
  };

  它的特点是：

  - 存在于内存
  - 放在全局 inode 表 itable 里
  - 除了磁盘 inode 信息，还额外保存运行时管理信息

  它比 dinode 多出来的关键字段是：

  - dev：属于哪个设备
  - inum：inode 编号
  - ref：当前有多少内核引用指向它
  - lock：保护 inode 内容的 sleep lock
  - valid：磁盘内容是否已经读入内存

  所以 inode 不是“磁盘格式”，而是：

  内核为了方便并发访问、缓存和生命周期管理而维护的运行时对象。

  ———

  ## 3. 它们最本质的区别

  最本质区别就是：

  - dinode 负责持久化
  - inode 负责运行时管理

  你可以这么理解：

  - dinode 像硬盘里的存档记录
  - inode 像内核手里正在使用的工作副本

  ———

  ## 4. 哪些字段是两者共有的

  两者共享的那部分字段，其实就是“文件本体元数据”：

  - type
  - major
  - minor
  - nlink
  - size
  - addrs[]

  这些字段会在内存和磁盘之间来回同步。

  比如：

  - ilock()：把 dinode 读到 inode
  - iupdate()：把 inode 写回 dinode

  ———

  ## 5. inode 为什么需要多出这些字段

  ### dev

  因为内核里可能缓存多个设备上的 inode，所以要知道属于哪个设备。

  ### inum

  磁盘 inode 在磁盘上是“按编号存放”的，所以内存 inode 也必须记住它对应哪个 inode 编号。

  ### ref

  表示这个内存 inode 当前被多少地方引用：

  - 打开文件对象
  - 当前工作目录
  - 路径解析过程中的临时引用

  没有 ref 就没法知道什么时候可以回收这个内存 inode 项。

  ### lock

  多个进程可能同时访问同一个 inode。
  要保护：

  - size
  - nlink
  - addrs[]
  - 内容读写过程

  所以要有 sleep lock。

  ### valid

  iget() 只是“拿到一个 inode 引用”，并不一定立刻读磁盘。
  valid 表示：

  - 0：内存副本还没从磁盘装载
  - 1：磁盘内容已经同步进来了

  这样内核就可以做 inode 缓存，而不必每次都立刻读盘。

  ———

  ## 6. 它们之间怎么转换

  ### 磁盘 -> 内存：ilock()

  在 kernel/fs.c:295 里：

  - 如果 ip->valid == 0
  - 就从磁盘读对应 dinode
  - 把 type / major / minor / nlink / size / addrs[] 拷进内存 inode
  - 然后设 valid = 1

  这一步是：

  把磁盘 inode 装载为可用的内存 inode

  ———

  ### 内存 -> 磁盘：iupdate()

  在 kernel/fs.c:228 里：

  - 把内存 inode 的持久化字段写回磁盘 dinode

  这一步是：

  把运行中修改过的 inode 元数据正式落盘

  ———

  ## 7. 回收时它们的差别

  ### inode 回收

  当内存中的 ref == 0 时，这个 inode 表项就可以被复用。

  也就是说，内存 inode 会被缓存和回收。

  ### dinode 回收

  只有当：

  - nlink == 0
  - 没有人再引用这个文件

  才会真正把磁盘 inode 清空（type = 0），表示这个磁盘 inode 重新空闲。

  所以：

  - inode 的回收是“内存缓存对象”的回收
  - dinode 的回收是“磁盘文件对象”的回收

  ———

  ## 8. 一个直观例子

  假设你打开文件 /a：

  1. 目录项 "a" 指向 inode 编号 inum = 5
  2. 内核通过 iget(dev, 5) 拿到一个内存 inode
  3. 如果它还没加载，就 ilock() 从磁盘把 dinode #5 读进来
  4. 后续读写都操作这个内存 inode
  5. 如果修改了 size/addrs，再通过 iupdate() 写回磁盘 dinode
  6. 文件关闭后，iput() 减少 ref
  7. 若以后没人再用，这个内存 inode 可被复用
  8. 若同时 nlink==0，则对应磁盘 dinode 也会被真正释放

  ———

  ## 9. 一张简表

   对比项                  dinode              inode
  ━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━
   所在位置                磁盘                内存
  ──────────────────────  ──────────────────  ────────────────────
   定义位置                fs.h                file.h
  ──────────────────────  ──────────────────  ────────────────────
   作用                    持久化文件元数据    运行时管理文件对象
  ──────────────────────  ──────────────────  ────────────────────
   是否包含锁              否                  是
  ──────────────────────  ──────────────────  ────────────────────
   是否包含引用计数        否                  是
  ──────────────────────  ──────────────────  ────────────────────
   是否包含设备号/编号     否                  是
  ──────────────────────  ──────────────────  ────────────────────
   是否直接参与并发控制    否                  是
  ──────────────────────  ──────────────────  ────────────────────
   是否断电后保留          是                  否

  ———

  ## 10. 一句话总结

  你可以直接记成：

  > dinode 是磁盘上的持久化 inode 结构，只保存文件元数据；inode 是内存中的运行时 inode 对象，在保存同样元数据的基础上，额外增加了设备号、inode 编号、引用计数、有效位和锁，用于缓存、
  > 同步和生命周期管理。

  如果你愿意，我下一步可以继续把 iget / ilock / iupdate / iput 这四个函数串起来讲成一条完整的 inode 生命周期。




› 解读file.c


• file.c 是 xv6 里“打开文件对象层”的实现。

  如果你前面已经看完了：

  - fs.c：文件系统本体，负责 inode、目录、路径、数据块
  - proc.h：每个进程有 ofile[NOFILE]

  那 file.c 解决的就是中间这一层：

  “进程打开一个文件后，内核里到底用什么对象表示这次打开？”

  答案就是 struct file。

  ———

  # 1. file.c 在整个文件系统里的位置

  可以把整个链路理解成：

  - fs.c 管的是“文件本体”
    也就是 inode、目录、路径、磁盘块

  - file.c 管的是“打开文件实例”
    也就是一次 open() 之后得到的那个内核对象

  - sysfile.c 管的是“系统调用入口”
    也就是 open/read/write/close

  所以：

  - inode 表示“这个文件是什么”
  - struct file 表示“这个文件现在被谁打开、以什么方式打开、当前偏移是多少”

  ———

  # 2. struct file 是什么

  定义在 kernel/file.h:1：

  struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
    int ref;
    char readable;
    char writable;
    struct pipe *pipe;
    struct inode *ip;
    uint off;
    short major;
  };

  这个结构是整个 file.c 的核心。

  字段含义：

  - type
    表示这是哪类“打开对象”
      - FD_PIPE：管道
      - FD_INODE：普通文件/目录
      - FD_DEVICE：设备文件

  - ref
    引用计数
    表示当前有多少 fd 或内核引用共享这个打开文件对象

  - readable / writable
    当前打开方式是否允许读/写

  - pipe
    如果这是管道，指向对应 struct pipe

  - ip
    如果这是普通文件或设备文件，指向对应 inode

  - off
    当前读写偏移
    这是“打开文件实例”的偏移，不是 inode 的全局属性

  - major
    若是设备文件，记录主设备号

  ———

  # 3. 为什么需要 struct file

  因为一个 inode 只能表示“文件本体”，但不能表示“这次打开”的状态。

  例如：

  - 同一个文件被两个进程分别 open
  - 它们可能：
      - 打开的权限不同
      - 当前偏移不同
      - 引用关系不同

  所以必须有一个独立层次来表示“打开实例”。

  这就是 struct file 的作用。

  最关键的是 off：

  - inode 里没有“当前读到哪里”
  - off 放在 struct file 里
  - 所以两个不同的 open() 得到两个不同 struct file，偏移互不影响
  - 但 dup() / fork() 共享同一个 struct file 时，偏移就共享

  这正是 Unix 语义。

  ———

  # 4. 全局文件表 ftable

  在 kernel/file.c:16：

  struct {
    struct spinlock lock;
    int nfile;
  } ftable;

  你这个仓库里，struct file 已经不是 xv6 原版的静态数组了，而是：

  - 用 nfile 统计当前已分配打开文件对象数量
  - 实际对象通过 kmalloc(sizeof(*f)) 动态分配

  这和你们前面做的 kmalloc 接入有关。

  它的作用是：

  - 维护全局打开文件对象总数
  - 约束不超过 NFILE

  所以现在的策略是：

  - 每个进程 fd 表有上限 NOFILE
  - 全局打开文件对象总数有上限 NFILE

  ———

  # 5. fileinit()

  kernel/file.c:22

  作用很简单：

  - 初始化 ftable.lock
  - 设置 ftable.nfile = 0

  这是打开文件对象层的初始化入口。

  ———

  # 6. filealloc()：分配一个打开文件对象

  kernel/file.c:30

  它的逻辑是：

  1. 拿 ftable.lock
  2. 检查当前 nfile 是否已经达到 NFILE
  3. 如果没满，先 nfile++
  4. 释放锁
  5. 用 kmalloc(sizeof(*f)) 分配一个新的 struct file
  6. 如果分配失败，再回滚 nfile--
  7. 清零结构体，设置 f->ref = 1
  8. 返回这个新对象

  这里有两个点很重要。

  ### 第一，filealloc() 分配的是 struct file

  不是 inode，不是 fd。

  也就是说，它分配的是：

  “一次打开文件实例”

  ### 第二，先占名额再 kmalloc

  这是为了保证全局上限控制正确。
  如果超过 NFILE，即使内存还有，也不允许再打开更多文件对象。

  ———

  # 7. filedup()：共享同一个打开文件对象

  kernel/file.c:58

  逻辑很简单：

  1. 拿锁
  2. 检查 ref >= 1
  3. ref++
  4. 释放锁
  5. 返回同一个 struct file *

  这用于：

  - dup(fd)
  - fork() 继承文件描述符

  重点要理解：

  filedup() 不会复制内容，不会新建一个 file 对象，而是共享同一个 struct file。

  所以共享的也包括：

  - off
  - readable/writable
  - ip 或 pipe

  这就是为什么：

  - dup() 后两个 fd 共享偏移
  - fork() 后父子进程共享同一个打开文件状态

  ———

  # 8. fileclose()：关闭打开文件对象

  kernel/file.c:70

  这是 file.c 最关键的函数之一。

  逻辑是：

  1. 拿锁
  2. 检查 ref < 1 则 panic
  3. --f->ref
  4. 如果 ref > 0，说明还有别人共享这个打开文件对象，直接返回
  5. 如果 ref == 0：
      - 先把整个 *f 拷贝到局部变量 ff
      - nfile--
      - 释放锁

  6. 根据 ff.type 真正释放底层资源：
      - FD_PIPE -> pipeclose(ff.pipe, ff.writable)
      - FD_INODE / FD_DEVICE -> begin_op(); iput(ff.ip); end_op();

  7. 最后 kmfree(f)

  ———

  ## 为什么要先拷贝到局部变量 ff

  这一点很经典。

  struct file ff;
  ff = *f;
  ...
  kmfree(f);

  原因是：

  - 后面要释放 f 本身
  - 但释放前还需要根据它的类型和指针字段做善后
  - 所以先把内容复制到栈上的 ff
  - 后续即使 f 被释放，ff 里还有完整信息

  ———

  ## fileclose() 关闭的不是“fd”，而是“打开文件对象”

  这点非常重要。

  - sys_close(fd) 先把进程 ofile[fd] 清空
  - 然后再调用 fileclose(f)

  所以：

  - fd 的消失是进程级别的
  - struct file 的真正释放是全局对象级别的
  - 只有 ref == 0 才真正销毁

  ———

  # 9. filestat()：拿文件元数据

  kernel/file.c:100

  作用：

  - 通过 struct file 取到底层 inode 的 stat 信息
  - 再把它拷贝到用户空间

  流程：

  1. 检查类型必须是 FD_INODE 或 FD_DEVICE
  2. ilock(f->ip)
  3. 调用 stati(f->ip, &st)
  4. iunlock(f->ip)
  5. 用 copyout() 把 struct stat 写到用户地址

  这里说明：

  - struct file 本身不保存完整 stat 信息
  - 真正的元数据还是在 inode 里

  ———

  # 10. fileread()：读打开文件对象

  kernel/file.c:121

  这是读路径的统一入口。

  先检查：

  if (f->readable == 0)
    return -1;

  然后按类型分流。

  ### FD_PIPE

  r = piperead(f->pipe, addr, n);

  ### FD_DEVICE

  r = devsw[f->major].read(1, addr, n);

  ### FD_INODE

  ilock(f->ip);
  if ((r = readi(f->ip, 1, addr, f->off, n)) > 0)
    f->off += r;
  iunlock(f->ip);

  这里最关键的是最后一句：

  f->off += r;

  这说明：

  - 普通文件的读取偏移保存在 struct file
  - 每次成功读完后推进偏移
  - 所以连续 read() 会顺序往后读

  ———

  # 11. filewrite()：写打开文件对象

  kernel/file.c:147

  结构与 fileread() 类似，也是先检查：

  if (f->writable == 0)
    return -1;

  然后按类型分流。

  ### 管道

  走 pipewrite()

  ### 设备

  走 devsw[major].write

  ### 普通 inode 文件

  这是最复杂的部分。

  int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;

  这行是在计算：

  一次日志事务里，单次最多安全写多少字节

  因为 xv6 的日志空间有限，不能把一个特别大的写一次性塞进一个事务里。

  所以 filewrite() 会把大写操作拆成多轮：

  1. 每轮取 n1 <= max
  2. begin_op()
  3. ilock(f->ip)
  4. writei(f->ip, 1, addr + i, f->off, n1)
  5. 成功后推进 f->off
  6. iunlock
  7. end_op()
  8. 循环直到写完全部数据

  这样做是为了避免：

  - 单次事务写太多块
  - 超过日志容量

  ———

  # 12. file.c 的核心抽象

  学 file.c 时最该抓住的一句话是：

  inode 表示文件本体，file 表示打开实例。

  区别如下：

  - inode
      - 对应磁盘上的文件对象
      - 管理文件元数据和数据块
      - 所有打开实例可能共享同一个 inode

  - file
      - 对应一次打开文件的运行时对象
      - 管理权限、当前偏移、引用计数
      - 可以有多个 file 指向同一个 inode

  所以 file.c 正是在做：

  把“文件本体”包装成“可被进程以 fd 使用的打开对象”

  ———

  # 13. file.c 和其它文件的关系

  ### 与 proc.h

  每个进程有：

  - ofile[NOFILE]

  里面放的是 struct file *

  ### 与 sysfile.c

  sysfile.c 负责：

  - 把 fd 取出来
  - 调 filealloc/filedup/fileclose/fileread/filewrite

  ### 与 fs.c

  当 struct file 类型是 FD_INODE 时：

  - 读写最终落到 readi/writei
  - 关闭最终落到 iput

  ### 与 pipe.c

  当类型是 FD_PIPE 时：

  - 读写最终走 piperead/pipewrite
  - 关闭最终走 pipeclose

  所以 file.c 就是：

  把普通文件、设备、管道统一成一个“打开文件对象接口层”

  ———

  # 14. 一句话总结

  你可以把 file.c 概括成：

  > file.c 实现了 xv6 的打开文件对象层。它用 struct file 统一表示普通文件、设备和管道的打开实例，负责维护引用计数、读写权限和当前偏移，并通过 filealloc/filedup/fileclose/fileread/
  > filewrite 为系统调用层提供统一接口。它位于进程文件描述符表与底层 inode/pipe/设备实现之间，是文件系统访问路径中的关键中间层。

  如果你愿意，我下一步可以继续按同样方式解读 sysfile.c，这样你就能把整条链：
  fd -> struct file -> inode -> data blocks
  彻底串起来。



› 解读 sysfile.c


• sysfile.c 是 xv6 里文件系统相关系统调用的入口层。

  如果前面你已经看过：

  - fs.c：inode、目录、路径、数据块
  - file.c：打开文件对象 struct file

  那 sysfile.c 做的就是最后一层：

  把用户态的 open/read/write/close/link/unlink/mkdir/chdir/pipe 这些系统调用，转成内核里的 file.c + fs.c 操作。

  核心文件是 kernel/sysfile.c:1。

  ———

  ## 1. 它在整体调用链里的位置

  整条链可以这样看：

  - 用户程序调用 open("a", O_CREATE|O_RDWR)
  - trap 进入内核
  - syscall.c 分发到 sys_open()
  - sys_open() 在 sysfile.c
  - sys_open() 再调用：
      - namei/nameiparent/create 这些 fs.c 函数
      - filealloc/fdalloc 这些 file.c 函数

  所以 sysfile.c 不是文件系统本体，而是：

  文件系统的系统调用适配层

  它主要做三件事：

  - 取用户参数
  - 做合法性检查
  - 调下层真正干活的函数

  ———

  ## 2. 最前面的两个辅助函数最重要

  ### argfd()

  见 kernel/sysfile.c:21

  static int
  argfd(int n, int *pfd, struct file **pf)

  作用：

  - 从第 n 个系统调用参数里取一个 fd
  - 检查它是否合法
  - 如果合法，顺便返回：
      - fd 本身
      - 对应的 struct file *

  核心检查是：

  if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
    return -1;

  也就是说，argfd() 实现了：

  用户态整数 fd -> 当前进程 ofile[] -> struct file *

  这是 read/write/close/fstat/dup 的共同入口。

  ———

  ### fdalloc()

  见 kernel/sysfile.c:39

  static int
  fdalloc(struct file *f)

  作用：

  - 在当前进程 ofile[] 中找第一个空槽
  - 把这个 struct file * 放进去
  - 返回对应 fd

  本质上是：

  内核打开文件对象 -> 分配一个用户可见的文件描述符整数

  所以：

  - argfd() 是 “fd -> file”
  - fdalloc() 是 “file -> fd”

  这两个函数是整个 sysfile.c 的关键纽带。

  ———

  ## 3. 最简单的几类系统调用

  ### sys_dup()

  见 kernel/sysfile.c:54

  逻辑：

  1. 用 argfd() 取出旧 fd 对应 struct file *
  2. 调 fdalloc(f) 分配一个新 fd
  3. 调 filedup(f) 增加 struct file 引用计数
  4. 返回新 fd

  重点：

  - dup 不新建 inode
  - 也不新建 struct file
  - 只是让两个 fd 共享同一个 struct file

  所以偏移 off 也是共享的。

  ———

  ### sys_read()

  见 kernel/sysfile.c:68

  逻辑：

  1. 取用户缓冲区地址 p
  2. 取字节数 n
  3. 用 argfd() 找到 struct file *
  4. 调 fileread(f, p, n)

  sysfile.c 本身不读 inode，不碰磁盘块，它只是把参数接到 file.c。

  ———

  ### sys_write()

  见 kernel/sysfile.c:82

  和 sys_read() 对称：

  1. 取缓冲区地址和长度
  2. 通过 fd 找 struct file
  3. 调 filewrite()

  ———

  ### sys_close()

  见 kernel/sysfile.c:97

  逻辑：

  1. 用 argfd() 找到 fd 和 struct file *
  2. 把当前进程 ofile[fd] = 0
  3. 调 fileclose(f)

  所以：

  - 从进程视角，fd 已经没了
  - 从全局打开文件对象视角，还要看 ref 是否归零决定是否真正释放

  ———

  ### sys_fstat()

  见 kernel/sysfile.c:110

  逻辑：

  1. 取用户态 struct stat * 地址
  2. 通过 fd 找到底层 struct file
  3. 调 filestat()

  ———

  ## 4. 链接与删除

  ### sys_link()

  见 kernel/sysfile.c:123

  作用是创建硬链接。

  逻辑主线：

  1. 从用户取 old 和 new 两个路径
  2. begin_op()
  3. namei(old) 找到旧 inode
  4. 禁止给目录建硬链接
  5. ip->nlink++，iupdate(ip)
  6. nameiparent(new, name) 找到新路径的父目录
  7. dirlink(dp, name, ip->inum) 把新名字挂进去
  8. 成功返回；失败则回滚 nlink--

  重点理解：

  - link 不复制文件内容
  - 只是给同一个 inode 增加一个新的目录项名字

  ———

  ### isdirempty()

  见 kernel/sysfile.c:172

  这是 unlink 的辅助函数。
  它跳过前两个目录项 . 和 ..，检查目录里是否还有别的有效项。

  ———

  ### sys_unlink()

  见 kernel/sysfile.c:188

  作用是删除目录项。

  逻辑：

  1. 取路径
  2. begin_op()
  3. nameiparent(path, name) 找父目录
  4. 禁止删除 "." 和 `".."``
  5. dirlookup(dp, name, &off) 找到目标 inode
  6. 如果目标是目录，要求它必须为空目录
  7. 把父目录中的这个 dirent 清零
  8. 若目标是目录，父目录 nlink--
  9. 目标 inode nlink--
  10. iunlockput/iput
  11. end_op()

  关键点：

  - unlink 删的是“名字”
  - 真正文件内容是否立刻回收，要看后续 iput() 是否发现它已经 nlink==0 且无人引用

  ———

  ## 5. create()：创建普通文件/目录/设备的共用核心

  见 kernel/sysfile.c:245

  这是 sys_open(O_CREATE)、sys_mkdir()、sys_mknod() 的公共底层。

  逻辑主线：

  1. nameiparent(path, name) 找父目录 inode
  2. 锁住父目录
  3. 如果名字已存在：
      - 如果要创建的是普通文件，且旧对象是普通文件或设备，可直接返回旧 inode
      - 否则失败

  4. 如果不存在：
      - ialloc(dp->dev, type) 分配新 inode
      - 初始化 major/minor/nlink
      - iupdate(ip)

  5. 如果是目录：
      - 建立 . 指向自己
      - 建立 .. 指向父目录

  6. 再把新名字写入父目录 dirlink(dp, name, ip->inum)
  7. 如果创建的是目录，还要给父目录 nlink++
  8. 返回新 inode

  这函数很关键，因为它把：

  - inode 分配
  - 目录项建立
  - 目录特殊处理

  都统一封装起来了。

  ———

  ## 6. sys_open()：最重要的系统调用之一

  见 kernel/sysfile.c:304

  它基本把整个文件访问链都串起来了。

  逻辑分几步。

  ### 第一步：取路径和打开模式

  argint(1, &omode);
  argstr(0, path, MAXPATH)

  ### 第二步：开始事务

  begin_op();

  因为打开文件如果涉及创建、截断等元数据更新，需要日志保护。

  ### 第三步：获得 inode

  - 如果 O_CREATE，调用 create(path, T_FILE, 0, 0)
  - 否则 namei(path) 找已有 inode

  ### 第四步：做类型检查

  if (ip->type == T_DIR && omode != O_RDONLY)
    ...

  目录只能只读打开，不能以写模式打开。

  如果是设备，还会检查 major 是否合法。

  ### 第五步：分配打开文件对象和 fd

  f = filealloc();
  fd = fdalloc(f);

  这一步非常关键：

  - inode 只是文件本体
  - filealloc() 创建的是这次打开的运行时对象
  - fdalloc() 才把它挂到进程 fd 表里

  ### 第六步：初始化 struct file

  如果是设备：

  - f->type = FD_DEVICE

  否则：

  - f->type = FD_INODE
  - f->off = 0

  然后统一设置：

  - f->ip = ip
  - f->readable
  - f->writable

  ### 第七步：处理截断

  if ((omode & O_TRUNC) && ip->type == T_FILE)
    itrunc(ip);

  所以 O_TRUNC 是 open 内部直接调用 itrunc() 完成的。

  ### 第八步：解锁并结束事务

  iunlock(ip);
  end_op();
  return fd;

  ———

  ## 7. sys_mkdir() 和 sys_mknod()

  ### sys_mkdir()

  见 kernel/sysfile.c:373

  逻辑非常短：

  1. begin_op()
  2. create(path, T_DIR, 0, 0)
  3. 成功后 iunlockput(ip)
  4. end_op()

  因为大部分复杂逻辑都被 create() 封装了。

  ———

  ### sys_mknod()

  见 kernel/sysfile.c:389

  用于建设备文件。

  本质上和 mkdir 类似，只是：

  - type 是 T_DEVICE
  - 需要额外的 major/minor

  ———

  ## 8. sys_chdir()

  见 kernel/sysfile.c:409

  逻辑：

  1. 取路径
  2. begin_op()
  3. namei(path) 找 inode
  4. 检查它必须是目录
  5. iput(p->cwd) 释放旧当前目录
  6. p->cwd = ip
  7. end_op()

  所以当前工作目录本质上是进程里保存的一个 inode 引用。

  ———

  ## 9. sys_exec() 为什么也在这里

  见 kernel/sysfile.c:434

  它虽然是程序加载，不完全属于“文件系统接口”，但因为它要：

  - 按路径找可执行文件
  - 处理用户传入的 argv

  所以放在 sysfile.c。

  它的主要工作是：

  1. 从用户态取出 path
  2. 从用户态 argv 指针数组里逐个取参数字符串
  3. 每个参数先拷到内核临时页里
  4. 调用 kexec(path, argv)
  5. 最后释放临时页

  真正 ELF 加载逻辑不在这里，而在 exec.c。

  ———

  ## 10. sys_pipe()

  见 kernel/sysfile.c:477

  它不是普通文件系统路径操作，但属于 fd/打开文件语义，所以也放在这里。

  逻辑：

  1. 取用户传入的两个 fd 数组地址
  2. 调 pipealloc(&rf, &wf) 创建两个 struct file：
      - 读端
      - 写端

  3. 分别 fdalloc(rf) 和 fdalloc(wf)
  4. 把两个 fd 拷回用户数组

  所以 pipe() 最终也复用了同一套：

  - struct file
  - ofile[]
  - fd 分配机制

  ———

  ## 11. sysfile.c 的核心思想

  学这个文件时，最重要的是抓住下面这句话：

  sysfile.c 不实现文件系统本体，它只是把“用户态请求”翻译成“file.c 和 fs.c 能理解的内核操作”。

  它本身做的多是：

  - argint/argstr/argaddr
  - argfd/fdalloc
  - begin_op/end_op
  - 调用下层函数

  所以它是“胶水层”，但又是非常关键的胶水层。

  ———

  ## 12. 你现在应该形成的完整链路

  看到 sysfile.c 后，整条文件系统访问链就应该串起来了：

  ### open

  sys_open
  -> namei/create
  -> filealloc
  -> fdalloc

  ### read

  sys_read
  -> argfd
  -> fileread
  -> readi

  ### write

  sys_write
  -> argfd
  -> filewrite
  -> writei
  -> bmap/balloc/log_write

  ### close

  sys_close
  -> ofile[fd]=0
  -> fileclose
  -> iput/pipeclose

  ### unlink

  sys_unlink
  -> nameiparent
  -> dirlookup
  -> 清目录项
  -> nlink--
  -> iput

  ———

  ## 13. 一句话总结

  你可以把 sysfile.c 概括成：

  > sysfile.c 是 xv6 文件系统相关系统调用的入口层，负责从用户态提取参数、检查文件描述符和路径合法性、控制日志事务边界，并把 open/read/write/close/link/unlink/mkdir/chdir/pipe/exec 等
  > 请求分发到 file.c 的打开文件对象层和 fs.c 的 inode/目录/路径层，是用户态文件接口与内核文件系统实现之间的桥梁。

  如果你愿意，我下一步可以继续做两件事之一：

  1. 把 open/read/write/close/unlink 五条调用链单独画成“从用户态到磁盘块”的流程。
  2. 把 file.c + sysfile.c 压缩成适合你写进 Maple/Done.md 的文档版。
