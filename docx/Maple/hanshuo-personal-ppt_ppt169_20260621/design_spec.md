# hanshuo-personal-ppt - Design Spec

> Human-readable design narrative — rationale, audience, style, color choices, content outline. Read once by downstream roles for context.
>
> Machine-readable execution contract: `spec_lock.md` (color / typography / icon / image short form). Executor re-reads `spec_lock.md` before every SVG page to resist context-compression drift. Keep both in sync; on divergence, `spec_lock.md` wins.

## I. Project Information

| Item | Value |
| ---- | ----- |
| **Project Name** | hanshuo-personal-ppt |
| **Canvas Format** | PPT 16:9 (1280×720) |
| **Page Count** | 15 |
| **Design Style** | `instructional` narrative mode + `swiss-minimal` visual style |
| **Target Audience** | 操作系统课程设计答辩老师与同学 |
| **Use Case** | xv6-riscv 课程设计个人答辩汇报，重点说明韩硕负责的系统启动、内存系统与文件系统扩展 |
| **Content Strategy** | 允许在现有实现基础上重组表达，但不引入超出项目实现的新事实。整体按“整体框架 → 基础能力 → 扩展实现 → 测试验证”的教学式路径展开，使老师先看到能力边界，再看到个人扩展和真实运行证据。 |
| **Created Date** | 2026-06-21 |

---

## II. Canvas Specification

| Property | Value |
| -------- | ----- |
| **Format** | PPT 16:9 |
| **Dimensions** | 1280×720 px |
| **viewBox** | `0 0 1280 720` |
| **Margins** | left/right 84px, top 64px, bottom 56px |
| **Content Area** | 1112×600 px |

---

## III. Visual Theme

### Theme Style

- **Mode**: `instructional`
- **Visual style**: `swiss-minimal`
- **Theme**: Light theme
- **Tone**: academic, technical, restrained, highly structured

### Color Scheme

| Role | HEX | Purpose |
| ---- | --- | ------- |
| **Background** | `#FFFFFF` | Page background |
| **Secondary bg** | `#F4F7FB` | Diagram/image container background |
| **Primary** | `#1E3A8A` | Page title, section line, key labels |
| **Accent** | `#F59E0B` | Single-point emphasis, test result highlight |
| **Accent bg** | `#FFF7E8` | Highlight strips behind conclusions |
| **Secondary accent** | `#3B82F6` | Auxiliary highlight, icon tint |
| **Body text** | `#111827` | Main text |
| **Secondary text** | `#4B5563` | Explanatory notes |
| **Tertiary text** | `#6B7280` | Captions, minor labels |
| **Border/divider** | `#D7E0EC` | Card border, image frame, separators |
| **Grid** | `#E5EAF2` | Hairline structure guides in Swiss layouts |
| **Success** | `#059669` | Positive test status |
| **Warning** | `#DC2626` | Failure/risk markers if needed |

### Gradient Scheme (if needed, using SVG syntax)

```xml
<linearGradient id="titleLine" x1="0%" y1="0%" x2="100%" y2="0%">
  <stop offset="0%" stop-color="#1E3A8A"/>
  <stop offset="100%" stop-color="#3B82F6"/>
</linearGradient>

<linearGradient id="softScrim" x1="0%" y1="0%" x2="100%" y2="0%">
  <stop offset="0%" stop-color="#FFFFFF" stop-opacity="0.94"/>
  <stop offset="100%" stop-color="#FFFFFF" stop-opacity="0.00"/>
</linearGradient>
```

---

## IV. Typography System

### Font Plan

**Typography direction**: modern CJK technical sans; single-family discipline consistent with `swiss-minimal`

| Role | Chinese | English | Fallback tail |
| ---- | ------- | ------- | ------------- |
| **Title** | `"Microsoft YaHei", "Noto Sans CJK SC"` | `Arial` | `sans-serif` |
| **Body** | `"Microsoft YaHei", "Noto Sans CJK SC"` | `Arial` | `sans-serif` |
| **Emphasis** | `"Microsoft YaHei", "Noto Sans CJK SC"` | `Arial` | `sans-serif` |
| **Code** | — | `Consolas, "Courier New"` | `monospace` |

**Per-role font stacks**:

- Title: `"Microsoft YaHei", "Noto Sans CJK SC", Arial, sans-serif`
- Body: `"Microsoft YaHei", "Noto Sans CJK SC", Arial, sans-serif`
- Emphasis: `"Microsoft YaHei", "Noto Sans CJK SC", Arial, sans-serif`
- Code: `Consolas, "Courier New", monospace`

### Font Size Hierarchy

**Baseline**: Body font size = 20px

| Purpose | Ratio to body | Example @ body=24 (relaxed) | Example @ body=18 (dense) | Weight |
| ------- | ------------- | --------------------------- | ------------------------- | ------ |
| Cover title (hero headline) | 2.5-5x | 60-120px | 45-90px | Bold / Heavy |
| Chapter / section opener | 2-2.5x | 48-60px | 36-45px | Bold |
| Page title | 1.5-2x | 36-48px | 27-36px | Bold |
| Hero number (consulting KPIs) | 1.5-2x | 36-48px | 27-36px | Bold |
| Subtitle | 1.2-1.5x | 29-36px | 22-27px | SemiBold |
| **Body content** | **1x** | **24px** | **18px** | Regular |
| Annotation / caption | 0.7-0.85x | 17-20px | 13-15px | Regular |
| Page number / footnote | 0.5-0.65x | 12-16px | 9-12px | Regular |

**Project anchor sizes**:

- `cover_title`: 52px
- `title`: 34px
- `subtitle`: 24px
- `body`: 20px
- `annotation`: 14px

### Formula Rendering Policy

- **Policy**: `text-only`
- **Reason**: 本套个人答辩 PPT 不包含公式推导页，所有技术内容以结构图、路径链路和测试日志为主。

---

## V. Layout Principles

### Page Structure

- **Header area**: 64-120px; page title, index tag, top rule
- **Content area**: 520-580px; diagram, screenshot, callout text, comparison blocks
- **Footer area**: 28-40px; page number or light caption when needed

### Layout Pattern Library (combine or break as content demands)

| Pattern | Suitable Scenarios |
| ------- | ----------------- |
| **Single column centered** | Cover, conclusion |
| **Asymmetric split (4:6 / 5:5)** | Diagram + explanation, screenshot + key points |
| **Top-bottom split** | Wide technical figure on top with analysis below |
| **Three-column cards** | Overview or module comparison |
| **Matrix grid (2×2)** | Basic capability breakdown |
| **Negative-space-driven** | Section emphasis, final takeaway |

### Spacing Specification

**Universal**:

| Element | Recommended Range | Current Project |
| ------- | ---------------- | --------------- |
| Safe margin from canvas edge | 40-60px | 56px |
| Content block gap | 24-40px | 28px |
| Icon-text gap | 8-16px | 10px |

**Card-based layouts**:

| Element | Recommended Range | Current Project |
| ------- | ---------------- | --------------- |
| Card gap | 20-32px | 24px |
| Card padding | 20-32px | 24px |
| Card border radius | 8-16px | 10px |
| Three-column card width | 360-380px each | 352px each after margins |

**Non-card containers**:

- Diagram pages use strong left alignment and wide white margins rather than stacked cards.
- Screenshot pages prefer one large image with 2-3 lean callout blocks instead of many small panels.
- Body line-height target: 1.45 on dense pages, 1.6 on breathing pages.

---

## VI. Icon Usage Specification

### Source

- **Built-in icon library**: `templates/icons/`
- **Usage method**: SVG placeholder `<use data-icon="library/icon-name" .../>`

### Recommended Icon List

| Purpose | Icon Path | Page |
| ------- | --------- | ---- |
| 启动入口 / 默认目录 | `tabler-outline/home-2` | P02, P05 |
| 路径解析 / 挂载边界 | `tabler-outline/route` | P11, P12 |
| 内存 / CPU | `tabler-outline/cpu-2` | P06, P08 |
| 测试与结果 | `tabler-outline/chart-bar` | P09, P14 |
| 磁盘 / 日志 | `tabler-outline/database` | P10, P12 |
| 目录 / 文件入口 | `tabler-outline/folder-open` | P10, P13 |
| 隐藏显示 | `tabler-outline/eye-off` | P13 |
| 终端日志 | `tabler-outline/terminal-2` | P05, P09, P14 |

---

## VII. Visualization Reference List

本套 PPT 不使用 `templates/charts/` 中的图表模板，也不生成需要坐标校准的数据图表页面。所有结构图均来自项目已提供的 PNG 素材，按图片资源放入页面；因此后续执行阶段无需运行 `verify-charts`。

| Page | Template | Path | Summary-quote (verbatim from `charts_index.json`) | Usage |
| ---- | -------- | ---- | ------------------------------------------------- | ----- |
| — | — | — | — | No chart-library template pages in this deck |

**Runners-up considered**:

- `process_flow` | rejected for P03: 启动链路图已经由项目自制图片完整表达，无需再用模板重画。
- `layered_architecture` | rejected for P06/P10: 内存层次图和文件系统层次图已有统一风格的现成图片素材。
- `timeline_horizontal` | rejected for P12: 该页核心是“日志机制 + 双盘命名空间”并列说明，不是单时间轴叙事。

---

## VIII. Image Resource List

| Filename | Dimensions | Ratio | Purpose | Type | Layout pattern | Acquire Via | Status | Reference | text_policy | page_role |
| -------- | ---------- | ----- | ------- | ---- | -------------- | ----------- | ------ | --------- | ----------- | --------- |
| p3_system_boot_flow.png | 2250×1265 | 1.78 | 展示 `_entry -> start() -> main() -> userinit() -> init -> sh` 启动链路 | Diagram | `#5 Top-band image + bottom multi-column text` | user | Existing | 项目自制系统启动流程图，用于启动整体框架页 |  |  |
| system_boot_terminal.png | 1714×1230 | 1.39 | 展示 xv6 启动日志、进入 shell 与默认目录切换证据 | Photography | `#3 Right-third image + left text body` | user | Existing | 真实启动截图，体现系统成功启动到 shell |  |  |
| p6_memory_overview.png | 2250×1265 | 1.78 | 展示页级分配、页表管理、内核堆分配三层结构 | Diagram | `#5 Top-band image + bottom multi-column text` | user | Existing | 项目自制内存系统总览图 |  |  |
| p8_memory_extensions.png | 2250×1265 | 1.78 | 展示运行时物理内存探测与 slab/kmalloc 扩展 | Diagram | `#2 Left-third image + right text body` | user | Existing | 项目自制内存扩展图 |  |  |
| kmalloctest_boot_log.png | 882×735 | 1.20 | 展示 `kmalloctest` 启动自测通过日志 | Photography | `#48 Side-by-side comparison (before/after, A/B, then/now)` | user | Existing | 真实终端截图，证明 kmalloc 主路径已在内核早期自检 |  |  |
| p10_filesystem_layers.png | 2250×1265 | 1.78 | 展示 `sysfile/file/fs/log/bio/virtio disk` 分层关系 | Diagram | `#5 Top-band image + bottom multi-column text` | user | Existing | 项目自制文件系统分层图 |  |  |
| p12_log_and_dualdisk.png | 2250×1265 | 1.78 | 展示日志事务步骤与 `/disk1` 静态挂载结构 | Diagram | `#2 Left-third image + right text body` | user | Existing | 项目自制日志与双盘结构图 |  |  |
| lseektest.png | 831×403 | 2.06 | 展示 `lseektest` 全部通过的终端结果 | Photography | `#59 Image as horizontal divider band` | user | Existing | 真实测试截图，覆盖 SEEK_SET/CUR/END 与文件大小获取 |  |  |
| hidden_color_ls.png | 669×673 | 0.99 | 展示隐藏文件与颜色规则在 `ls`/`ls -a` 下的可见效果 | Photography | `#19 Image floating in whitespace with thin frame and caption` | user | Existing | 真实目录浏览截图，体现隐藏和着色策略 |  |  |

---

## IX. Content Outline

### Part 1: 封面与汇报总览

#### Slide 01 - Cover

- **Cover impact**: 用“从 xv6 基础内核到可答辩的扩展系统”作为核心钩子，采用大标题 + 三模块竖向分栏构图，让老师一眼看到汇报范围和个人负责边界。
- **Layout**: typographic poster with three vertical module labels and a restrained top rule
- **Title**: 基于 xv6-riscv 的系统启动、内存系统与文件系统扩展
- **Subtitle**: 操作系统课程设计 A 题个人汇报
- **Info**: 韩硕 / xv6-riscv / 2026-06

#### Slide 02 - 个人负责内容总览

- **Layout**: three-column overview
- **Title**: 个人负责范围与汇报结构
- **Core message**: 本次个人答辩围绕三个模块展开，并统一按照“框架、基础、扩展、测试”四层组织。
- **Content**:
  - 本人负责模块：系统启动 / 内存系统 / 文件系统
  - 组织方式：整体框架 → 基础实现 → 扩展实现 → 测试验证
  - 汇报目标：对齐课程要求、突出个人扩展、给出真实运行证据

### Part 2: 系统启动

#### Slide 03 - 系统启动整体框架

- **Layout**: top-band figure with concise analysis blocks below
- **Title**: 系统启动整体框架
- **Core message**: xv6 的启动链路已经从 QEMU 加载、特权级切换、内核初始化一路贯通到 shell。
- **Content**:
  - 主链路：`_entry -> start() -> main() -> userinit() -> init -> sh`
  - `entry.S` 完成最早期栈准备与 DTB 指针保存，`start()` 完成 M 态到 S 态切换
  - `main()` 继续完成控制台、内存、页表、设备和文件系统初始化，最终进入 shell

#### Slide 04 - 系统启动基础功能

- **Layout**: 2×2 matrix with short explanations
- **Title**: 系统启动基础功能
- **Core message**: 启动模块已经覆盖课程要求中的关键基础指标，而不是只停留在“能打印一行日志”。
- **Content**:
  - 从 `M` 态切换到 `S` 态：设置 `mstatus.MPP`、`mepc`、PMP 与委托寄存器
  - 为每个 hart 建立独立启动栈，保证 C 语言运行环境稳定
  - 通过 `stvec` 与 trap vector 完成异常/中断入口准备
  - 成功跳转 `main()`，创建首个用户进程并进入交互式 shell

#### Slide 05 - 系统启动扩展与验证

- **Layout**: screenshot-led breathing page with compact callouts
- **Title**: 系统启动扩展与验证
- **Core message**: 启动阶段不仅完成进入内核，还为后续内存探测、自测和用户目录体验预留了入口。
- **Content**:
  - 扩展一：保存 DTB 指针，供后续 `/memory/reg` 运行时物理内存探测使用
  - 扩展二：启动日志中接入 `kmalloctest` 自测，尽早发现堆分配异常
  - 扩展三：shell 默认进入 `/desktop`，根目录保留系统入口与挂载点 `/disk1`

### Part 3: 内存系统

#### Slide 06 - 内存系统整体结构

- **Layout**: top-band figure with three explanatory columns
- **Title**: 内存系统整体结构
- **Core message**: 当前内存系统已经形成“页框分配 + Sv39 页表 + 内核堆分配”的三层结构。
- **Content**:
  - 物理页框分配：`kalloc/kfree` 提供 4KB 页级资源
  - 虚拟内存管理：页表创建、映射、解除映射、用户地址空间维护
  - 内核堆分配器：`kmalloc/kmfree` 负责小对象精确分配与大对象回退

#### Slide 07 - 内存系统基础功能

- **Layout**: asymmetric split with left summary and right capability grid
- **Title**: 内存系统基础功能
- **Core message**: 在 xv6 原有机制上，页表、页分配和用户空间管理已经形成可运行的完整基础能力。
- **Content**:
  - 已具备物理内存管理、空闲链表页框分配、Sv39 三级页表和用户地址空间布局
  - `uvmalloc/uvmunmap/uvmfree` 等路径保证地址映射与回收可用
  - lazy `sbrk` 支持缺页时按需补页，使堆扩展不必立即分配物理页

#### Slide 08 - 内存系统扩展

- **Layout**: left figure + right stacked prose blocks
- **Title**: 内存系统扩展：运行时探测与 kmalloc
- **Core message**: 本人负责的关键增量在于把“写死上界”的教学实现推进为“可自适应内存规模”的运行版本，并补齐内核堆分配器。
- **Content**:
  - 运行时物理内存探测：启动时读取 DTB `/memory/reg`，不再写死 RAM 上界，`QEMU -m 128M/256M` 均可自动适配
  - `kmalloc/kmfree` 两级分配器：7 个 slab 档位，小对象走 slab，大对象回退整页分配
  - 真实接入：`struct pipe` 与 `struct file` 已改为精确分配，空 slab 页可自动归还

#### Slide 09 - 内存系统测试与验证

- **Layout**: screenshot + result bullets
- **Title**: 内存系统测试与验证
- **Core message**: `kmalloctest` 和真实对象路径回归说明内核堆分配器已经进入主路径，而不是孤立测试代码。
- **Content**:
  - 启动日志出现 `kmalloctest: ok`，说明边界检查、读写检查、复用检查和跨页增长检查均通过
  - `pipe` 与 `file` 对象已使用 `kmalloc`，证明修改进入真实内核调用路径
  - 运行时物理内存探测日志与不同 `-m` 配置的自适应行为共同验证了扩展有效

### Part 4: 文件系统

#### Slide 10 - 文件系统整体结构

- **Layout**: top-band figure with layered explanation
- **Title**: 文件系统整体结构
- **Core message**: 文件系统可以按“系统调用层 → 打开文件对象层 → inode/路径层 → 日志层 → 块设备层”理解其主干。
- **Content**:
  - `sysfile.c` 面向用户态提供 `open/read/write/close/chdir` 等接口
  - `file.c` 管理打开文件对象与文件描述符映射，`fs.c` 负责 inode、目录项和路径解析
  - `log.c` 负责崩溃一致性，`bio.c` 与 virtio disk 负责块缓存和磁盘 I/O

#### Slide 11 - 文件系统基础功能

- **Layout**: two-column explanation
- **Title**: 文件系统基础功能
- **Core message**: 在扩展之前，文件系统已经具备从路径解析到打开文件对象再到底层 inode 的完整基本链路。
- **Content**:
  - 支持文件/目录创建、删除、打开、关闭、读写，以及文件描述符表管理
  - 同时支持绝对路径和相对路径解析，目录层级已满足课程要求
  - 已补齐 `lseek`，覆盖 `SEEK_SET`、`SEEK_CUR`、`SEEK_END`，可用于随机访问和文件大小获取

#### Slide 12 - 文件系统扩展一：日志机制与双独立磁盘

- **Layout**: left figure + right prose blocks
- **Title**: 文件系统扩展一：日志机制与双独立磁盘
- **Core message**: 文件系统扩展的关键不是多接一个 `.img`，而是把日志、superblock、inode 和路径解析都调整为按设备解释。
- **Content**:
  - 日志机制：`begin_op()/end_op()` 管理事务边界，`log_write()/commit()/recover_from_log()` 保证多块更新的一致性
  - 双独立磁盘：根盘 `fs.img`，第二盘 `fs1.img`，文件不能跨盘分配
  - 命名空间设计：`/disk1` 作为静态挂载点，`cd /disk1; cd ..` 返回根盘 `/`，并禁止删除 `/disk1`

#### Slide 13 - 文件系统扩展二：隐藏文件与颜色显示

- **Layout**: left rule summary + right category blocks
- **Title**: 文件系统扩展二：隐藏与颜色显示
- **Core message**: 隐藏和着色被实现为 `ls` 的展示策略，因此不改磁盘格式，也不破坏双盘语义。
- **Content**:
  - 隐藏规则：`.`、`..`、点文件，以及根目录中预装的系统入口默认隐藏；`ls -a` 展开
  - 颜色规则：目录蓝色、设备文件黄色、隐藏项灰色，普通文件保持默认色
  - 实现特点：逻辑集中在用户态 `ls`，不修改 inode 元数据，不影响路径解析与存储语义

#### Slide 14 - 文件系统测试与验证

- **Layout**: dual-screenshot dense page
- **Title**: 文件系统测试与验证
- **Core message**: `lseek`、双盘访问以及隐藏/颜色显示都已经通过真实命令路径验证。
- **Content**:
  - `lseektest: ALL TESTS PASSED`，覆盖三种 whence 和文件大小获取
  - 第二盘测试中，`open/create/write/read` 与日志提交均可在 `/disk1` 路径下独立完成
  - `ls` 与 `ls -a` 的截图说明隐藏规则和颜色规则已经按预期工作

### Part 5: 总结

#### Slide 15 - 总结

- **Closing impact**: 用“三个模块、三类扩展、三组真实证据”收束全场，强调本次工作是在 xv6 基础上完成理解、扩展和验证，而不是脱离实际代码的概念设计。
- **Layout**: negative-space-driven closing page with three concise takeaway blocks
- **Content**:
  - 系统启动：完成从 QEMU 到 shell 的稳定引导，并补充 DTB 保存、启动自测与默认 `/desktop`
  - 内存系统：形成运行时探测 + 页级分配 + Sv39 页表 + `kmalloc` 两级架构
  - 文件系统：在基本路径之上扩展 `lseek`、日志理解、双独立磁盘以及隐藏/颜色显示

---

## X. Speaker Notes Requirements

One speaker note file per page, saved to `notes/`:

- **Filename**: match SVG name (e.g., `01_cover.md`)
- **Content**: each note should contain 3 parts — page purpose, speaking path, transition sentence
- **Timing**: cover/overview/conclusion 25-35s each; module pages 40-60s each; keep total within 12-15 minutes

---

## XI. Technical Constraints Reminder

### SVG Generation Must Follow:

1. viewBox: `0 0 1280 720`
2. Background uses `<rect>` elements
3. Text wrapping uses `<tspan>` (`<foreignObject>` FORBIDDEN)
4. Transparency uses `fill-opacity` / `stroke-opacity`; `rgba()` FORBIDDEN
5. FORBIDDEN: `mask`, `<style>`, `class`, `foreignObject`
6. FORBIDDEN: `textPath`, `animate*`, `script`
7. Text characters: write typography & symbols as raw Unicode; escape XML reserved chars as `&amp;` `&lt;` `&gt;` `&quot;` `&apos;`
8. `clipPath` allowed only on `<image>` elements; use native shapes for all other geometry

### PPT Compatibility Rules:

- `<g opacity="...">` FORBIDDEN; set opacity on each child
- Image transparency uses overlay mask layer
- Inline styles only; external CSS and `@font-face` FORBIDDEN
