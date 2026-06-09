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
