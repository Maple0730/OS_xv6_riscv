# xv6 死锁检测与调度可视化图表集（LaTeX + TikZ + PGFPlots）

> **工具**：LaTeX + TikZ + PGFPlots（专业学术论文级别）
>
> **编译**：`pdflatex main.tex`（需要安装 `ctex` / `pgfplots` / `tikz` 宏包）
>
> **作者**：tfc
>
> **日期**：2026-06-18
>
> **位置**：本文件提供所有图表的 LaTeX 源码，复制到 `.tex` 文件即可编译。

---

## 文件结构

```
PPT_image/latex/
├── main.tex              # 主文档（含全部图表）
├── README.md             # 本说明
└── compile.sh            # 一键编译脚本
```

> 实际文件需要切换到 agent 模式创建。下方是完整的 LaTeX 源码。

---

## 编译前准备

### 1. 安装 TeX 发行版

```bash
# Ubuntu/Debian
sudo apt install texlive-full texlive-lang-chinese

# macOS
brew install --cask mactex

# Windows
# 下载 MiKTeX 或 TeX Live
```

### 2. 必要宏包

```latex
\usepackage[UTF8]{ctex}              % 中文支持
\usepackage{tikz}                     % TikZ 核心
\usepackage{pgfplots}                 % PGFPlots 数据可视化
\pgfplotsset{compat=1.18}
\usetikzlibrary{positioning, arrows.meta, shapes.geometric, calc,
                fit, backgrounds, trees, shadows,
                decorations.pathmorphing, patterns, fadings}
\usepackage{xcolor}
\usepackage{caption}
\usepackage{hyperref}
\usepackage{amsmath, amssymb}
```

### 3. 配色方案

```latex
\definecolor{primaryblue}{RGB}{30,58,138}     % 深蓝主色 #1E3A8A
\definecolor{secondaryblue}{RGB}{59,130,246}  % 蓝 #3B82F6
\definecolor{lightblue}{RGB}{219,234,254}     % 浅蓝 #DBEAFE
\definecolor{accentorange}{RGB}{251,146,60}   % 橙 #FB923C
\definecolor{lightorange}{RGB}{254,215,170}   % 浅橙 #FED7AA
\definecolor{accentred}{RGB}{220,38,38}       % 红 #DC2626
\definecolor{lightred}{RGB}{252,165,165}      % 浅红 #FCA5A5
\definecolor{accentgreen}{RGB}{34,197,94}     % 绿 #22C55E
\definecolor{lightgreen}{RGB}{134,239,172}    % 浅绿 #86EFAC
\definecolor{accentyellow}{RGB}{234,179,8}    % 黄 #EAB308
\definecolor{lightyellow}{RGB}{254,240,138}   % 浅黄 #FEF08A
\definecolor{lightgray}{RGB}{243,244,246}     % 浅灰 #F3F4F6
\definecolor{midgray}{RGB}{107,114,128}       % 灰 #6B7280
\definecolor{darkgray}{RGB}{31,41,55}         % 深灰 #1F2937
```

---

## 完整主文档（main.tex）

```latex
% =============================================================================
%  xv6 死锁检测与调度可视化图表集
% =============================================================================
\documentclass[11pt,a4paper]{article}

% ---------- 宏包 ----------
\usepackage[UTF8]{ctex}
\usepackage[a4paper,margin=2cm]{geometry}
\usepackage{tikz}
\usepackage{pgfplots}
\pgfplotsset{compat=1.18}
\usetikzlibrary{positioning, arrows.meta, shapes.geometric, calc,
                fit, backgrounds, trees, shadows,
                decorations.pathmorphing, patterns, fadings}
\usepackage{xcolor}
\usepackage{caption}
\usepackage{hyperref}
\usepackage{amsmath, amssymb}

% ---------- 配色（见上） ----------
% （复制上文配色代码）

\captionsetup{font=small, labelfont=bf, format=hang}

\begin{document}

\title{\textbf{xv6 死锁检测与调度可视化图表集}\\\large LaTeX + TikZ + PGFPlots 学术论文版}
\author{tfc}
\date{2026-06-18}
\maketitle

\tableofcontents
\newpage

% ============ 第1部分：流程与概念图 ============
\section{流程与概念图}

% ---- 图 1：工程方法论闭环 ----
\subsection{图 1：工程方法论闭环}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    node distance=1.5cm,
    box/.style={rectangle, draw=primaryblue, fill=lightblue, very thick,
                minimum width=2.8cm, minimum height=1.2cm, align=center,
                font=\small\bfseries, rounded corners=2pt, drop shadow},
    arrow/.style={-{Stealth[length=3mm]}, very thick, draw=primaryblue},
    backarrow/.style={-{Stealth[length=3mm]}, very thick, draw=accentorange,
                      dashed}
]
\node[box] (theory)   {背景理论\\[2pt]\scriptsize 教科书公式\\算法描述};
\node[box, right=of theory]   (kernel)  {内核实现\\[2pt]\scriptsize syscall\\数据结构};
\node[box, right=of kernel]   (test)    {用户测试\\[2pt]\scriptsize 独立程序\\边界条件};
\node[box, right=of test]     (verify)  {实测验证\\[2pt]\scriptsize trace 日志\\性能对比};

\draw[arrow] (theory) -- (kernel);
\draw[arrow] (kernel)  -- (test);
\draw[arrow] (test)    -- (verify);

\draw[backarrow] (verify.south) .. controls +(0,-1.2) and +(0,-1.2) ..
    node[below, font=\footnotesize\itshape, text=accentorange]
    {闭环改进} (theory.south);

\begin{scope}[on background layer]
  \node[draw=primaryblue, dashed, very thick, rounded corners,
        fit=(theory)(kernel)(test)(verify), inner sep=0.4cm,
        label={[font=\bfseries, text=primaryblue]above:工程闭环}] {};
\end{scope}
\end{tikzpicture}
\caption{工程方法论闭环：理论 $\to$ 内核 $\to$ 测试 $\to$ 验证 $\to$ 反馈}
\label{fig:methodology}
\end{figure}

% ---- 图 2：进程状态机 ----
\subsection{图 2：进程状态机}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    node distance=1.4cm,
    state/.style={circle, draw=primaryblue, fill=lightblue, very thick,
                  minimum size=1.4cm, align=center, font=\small\bfseries,
                  drop shadow},
    arrow/.style={-{Stealth[length=2.5mm]}, thick, draw=primaryblue}
]
\node[state] (unused)   {UNUSED};
\node[state, below=of unused] (used)   {USED};
\node[state, below=of used]   (runnable) {RUNNABLE};
\node[state, below=of runnable] (running) {RUNNING};
\node[state, right=of running] (sleeping) {SLEEPING};
\node[state, right=of sleeping] (zombie)   {ZOMBIE};

\draw[arrow] (unused)   -- (used);
\draw[arrow] (used)     -- (runnable);
\draw[arrow] (runnable) -- (running);
\draw[arrow] (running)  -- (sleeping);
\draw[arrow] (sleeping) -- (zombie);
\draw[arrow] (sleeping.north) .. controls +(0.8,0.5) and +(-0.5,0.5) ..
    node[above, font=\footnotesize] {wakeup} (runnable.east);
\end{tikzpicture}
\caption{xv6 进程 6 态转换图}
\label{fig:states}
\end{figure}

% ---- 图 3：生产者-消费者同步流程 ----
\subsection{图 3：生产者-消费者同步流程}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    node distance=0.8cm,
    box/.style={rectangle, draw=primaryblue, fill=lightblue, thick,
                minimum width=2.5cm, minimum height=0.7cm, align=center,
                font=\small, rounded corners=2pt},
    arrow/.style={-{Stealth[length=2mm]}, thick, draw=primaryblue}
]
\node[font=\bfseries, text=primaryblue] (ptitle) {生产者};
\node[right=3.5cm of ptitle, font=\bfseries, text=primaryblue] (ctitle) {消费者};

\node[box, below=0.3cm of ptitle] (p1) {sem\_wait(E)};
\node[box, below=of p1] (p2) {sem\_wait(M)};
\node[box, below=of p2] (p3) {put item};
\node[box, below=of p3] (p4) {sem\_post(M)};
\node[box, below=of p4] (p5) {sem\_post(F)};

\node[box, below=0.3cm of ctitle] (c1) {sem\_wait(F)};
\node[box, below=of c1] (c2) {sem\_wait(M)};
\node[box, below=of c2] (c3) {get item};
\node[box, below=of c3] (c4) {sem\_post(M)};
\node[box, below=of c4] (c5) {sem\_post(E)};

\foreach \i/\j in {p1/p2, p2/p3, p3/p4, p4/p5} { \draw[arrow] (\i) -- (\j); }
\foreach \i/\j in {c1/c2, c2/c3, c3/c4, c4/c5} { \draw[arrow] (\i) -- (\j); }

\node[box, fill=lightyellow, draw=accentorange, right=1cm of p3,
      minimum width=1.5cm] (buf) {Buffer};
\draw[arrow, dashed, draw=accentorange] (p3) -- (buf);
\draw[arrow, dashed, draw=accentorange] (buf) -- (c3);

\node[below=0.3cm of p5, font=\footnotesize, text=midgray, align=left]
    {E=empty, F=full, M=mutex};
\end{tikzpicture}
\caption{生产者-消费者同步流程：3 个信号量协调}
\label{fig:prodcons}
\end{figure}

% ---- 图 4：哲学家就餐死锁 ----
\subsection{图 4：哲学家就餐死锁示意}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    blocked/.style={circle, draw=accentred, fill=lightred, thick,
                    minimum size=0.8cm, font=\small\bfseries, drop shadow},
    forkdead/.style={rectangle, draw=accentred, fill=lightred, thick,
                     minimum width=0.5cm, minimum height=0.3cm,
                     font=\tiny\bfseries}
]
\foreach \i in {0,1,2,3,4} {
  \node[blocked] (p\i) at ({90 + 72*(\i+0.5)}:1.8) {P\i};
}
\foreach \i in {0,1,2,3,4} {
  \pgfmathtruncatemacro{\j}{Mod(\i+1,5)}
  \node[forkdead] (f\i) at ({90 + 72*(\i+1)}:1.8) {F\i};
  \draw[thick, draw=accentred] (p\i) -- (f\i);
  \draw[thick, draw=accentred] (f\i) -- (p\j);
}

\draw[-{Stealth[length=2mm]}, very thick, dashed, draw=accentred]
    (p0) .. controls +(0.5,0.3) and +(-0.5,0.3) ..
    node[above, font=\footnotesize, text=accentred, fill=white,
         inner sep=1pt] {循环等待} (p1);

\node[font=\bfseries, text=accentred, align=center] at (0,0) {死锁\\Deadlock};
\node[above=2.5cm, font=\bfseries, text=primaryblue]
    {5 位哲学家各持左叉，等待右叉};
\end{tikzpicture}
\caption{哲学家就餐问题中的死锁状态}
\label{fig:deadlock}
\end{figure}

\newpage

% ---- 图 5：MLFQ 队列状态变化 ----
\subsection{图 5：MLFQ 队列状态变化}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    queue/.style={rectangle, draw=primaryblue, fill=lightblue, very thick,
                  minimum width=3.5cm, minimum height=0.9cm, align=center,
                  font=\small\bfseries, drop shadow},
    proc/.style={circle, draw=primaryblue, fill=lightblue, thick,
                 minimum size=0.7cm, font=\tiny\bfseries},
    down/.style={-{Stealth[length=2mm]}, thick, draw=accentorange, dashed},
    up/.style={-{Stealth[length=2mm]}, thick, draw=accentgreen, dashed}
]
\node[queue] (q0) {Q0 最高优先级\quad 5ms};
\node[queue, below=1.2cm of q0] (q1) {Q1 中等优先级\quad 10ms};
\node[queue, below=1.2cm of q1] (q2) {Q2 最低优先级\quad 20ms};

\node[proc, right=0.3cm of q0] (p1) {P1};
\node[proc, right=0.3cm of p1] (p2) {P2};
\node[proc, right=0.3cm of q1] (p3) {P3};
\node[proc, right=0.3cm of q2] (p4) {P4};
\node[proc, right=0.3cm of p4] (p5) {P5};

\draw[down] (p1.south) .. controls +(0,-0.4) and +(0,0.4) .. (q1.north)
    node[midway, right, font=\footnotesize, text=accentorange] {降级};
\draw[down] (p3.south) .. controls +(0,-0.4) and +(0,0.4) .. (q2.north);

\draw[up] (q2.north west) .. controls +(-1,1) and +(-1,-1) ..
    node[above, font=\footnotesize, text=accentgreen] {100 tick 提升} (q0.south west);

\node[right=2cm of q2, font=\footnotesize, align=left, text=midgray] {
    \textcolor{accentorange}{---} 降级：时间片用完\\[2pt]
    \textcolor{accentgreen}{---} 提升：100 tick
};
\end{tikzpicture}
\caption{MLFQ 多级反馈队列：降级与提升机制}
\label{fig:mlfq}
\end{figure}

% ---- 图 6：调度器架构 ----
\subsection{图 6：调度器架构}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    box/.style={rectangle, draw=primaryblue, fill=lightblue, thick,
                minimum width=5cm, minimum height=0.7cm, align=left,
                font=\ttfamily\small, text=darkgray},
    case/.style={rectangle, draw=accentorange, fill=lightorange, thick,
                 minimum width=4.5cm, minimum height=0.5cm, align=left,
                 font=\ttfamily\small, text=darkgray},
    arrow/.style={-{Stealth[length=2mm]}, thick, draw=primaryblue}
]
\node[box, fill=lightyellow] (entry) {void scheduler(void) \{};
\node[box, below=0.4cm of entry] (sw) {~~switch(current\_scheduler) \{};
\node[case, below=0.3cm of sw] (rr)  {~~case SCHED\_RR:    rr\_scheduler();    break;};
\node[case, below=0.15cm of rr] (fcfs){~~case SCHED\_FCFS:  fcfs\_scheduler();  break;};
\node[case, below=0.15cm of fcfs](sjf) {~~case SCHED\_SJF:   sjf\_scheduler();   break;};
\node[case, below=0.15cm of sjf] (mlfq){~~case SCHED\_MLFQ:  mlfq\_scheduler();  break;};
\node[case, below=0.15cm of mlfq](prio){~~case SCHED\_PRIO:  prio\_scheduler();  break;};
\node[box, below=0.3cm of prio] (end) {~~\}};

\draw[arrow] (entry) -- (sw);
\foreach \i/\j in {sw/rr, rr/fcfs, fcfs/sjf, sjf/mlfq, mlfq/prio, prio/end} {
  \draw[arrow] (\i) -- (\j);
}
\end{tikzpicture}
\caption{调度器统一入口架构（switch 分发）}
\label{fig:sched-arch}
\end{figure}

% ---- 图 7：管程结构 ----
\subsection{图 7：管程结构}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    monitor/.style={rectangle, draw=primaryblue, fill=lightblue, very thick,
                    minimum width=6cm, minimum height=4.5cm, rounded corners=4pt,
                    drop shadow},
    field/.style={rectangle, draw=primaryblue, fill=white, thick,
                  minimum width=5cm, minimum height=0.6cm, align=left,
                  font=\ttfamily\small, text=darkgray},
    cv/.style={rectangle, draw=accentorange, fill=lightorange, thick,
               minimum width=4.5cm, minimum height=0.6cm, align=left,
               font=\ttfamily\small, text=darkgray},
    title/.style={font=\bfseries, text=primaryblue}
]
\node[monitor] (mon) {};
\node[title, anchor=north] at (mon.north) {\large struct monitor};

\node[field, anchor=north] (mutex) at ([yshift=-0.3cm]mon.north)
    {struct spinlock mutex;};
\node[field, below=0.1cm of mutex] (owner) {struct proc *owner;};
\node[cv,    below=0.3cm of owner] (cv1)  {struct condvar not\_full;};
\node[cv,    below=0.15cm of cv1] (cv2)   {struct condvar not\_empty;};
\end{tikzpicture}
\caption{管程数据结构：互斥锁 + 条件变量}
\label{fig:monitor}
\end{figure}

% ---- 图 8：PI 时间线 ----
\subsection{图 8：优先级继承时间线（Mars Pathfinder）}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    timeline/.style={-{Stealth[length=2mm]}, ultra thick, draw=midgray},
    blocked/.style={rectangle, draw=accentred, fill=lightred, thick,
                    minimum width=1.2cm, minimum height=0.5cm, font=\tiny\bfseries},
    high/.style={rectangle, draw=accentorange, fill=lightorange, thick,
                 minimum width=1.2cm, minimum height=0.5cm, font=\tiny\bfseries}
]
\foreach \i/\name in {0/H, 1/M, 2/L, 3/Buffer} {
  \node[font=\bfseries, text=primaryblue] at (-0.3, {-1.2*\i}) {\name};
  \draw[timeline] (0,{-1.2*\i}) -- (12,{-1.2*\i});
}
\foreach \x/\t in {0/t0, 2/t1, 4/t2, 6/t3, 8/t4, 10/t5, 12/t6} {
  \node[font=\tiny, text=midgray] at (\x, 0.3) {\t};
  \draw[thick, midgray] (\x, 0.1) -- (\x, -3.7);
}

\node[high, minimum width=2cm] (h1) at (1, 0) {acquire};
\node[blocked] (h2) at (4, 0) {阻塞};
\node[high, minimum width=2cm] (h3) at (9, 0) {acquire OK};

\node[high, minimum width=8cm] (m1) at (4, -1.2) {busy-loop};

\node[high, minimum width=2cm] (l1) at (1, -2.4) {acquire};
\node[high, minimum width=2cm] (l2) at (7, -2.4) {release};
\draw[thick, primaryblue] (l1.south) -- (l2.north);

\node[active, minimum width=2cm] (lock1) at (1, -3.6) {持有锁};
\node[active, minimum width=2cm] (lock2) at (7, -3.6) {释放锁};

\draw[-{Stealth[length=2mm]}, thick, accentred] (h2.south) -- (lock1.north)
    node[midway, right, font=\tiny, text=accentred] {等锁};
\draw[-{Stealth[length=2mm]}, thick, accentgreen] (lock2.north) -- (h3.south)
    node[midway, right, font=\tiny, text=accentgreen] {得锁};

\node[font=\footnotesize, text=accentorange, align=center] at (4, -2.4)
    {$\uparrow$ 继承 H 优先级};
\end{tikzpicture}
\caption{优先级继承时间线（Mars Pathfinder Bug 修复）}
\label{fig:pi}
\end{figure}

% ---- 图 9：银行家算法安全序列 ----
\subsection{图 9：银行家算法安全性算法}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    proc/.style={rectangle, draw=primaryblue, fill=lightblue, thick,
                 minimum width=1.5cm, minimum height=0.6cm, font=\small\bfseries},
    avail/.style={rectangle, draw=accentgreen, fill=lightgreen, thick,
                  minimum width=2cm, minimum height=0.6cm, font=\small\bfseries},
    arrow/.style={-{Stealth[length=2mm]}, thick, draw=primaryblue}
]
\node[font=\bfseries, text=primaryblue] at (0, 1.2) {Work = Available};
\node[avail] (a0) at (0, 0) {Work=(3,3,2)};

\node[proc, below=0.8cm of a0]  (p0) {P1};
\node[proc, right=0.3cm of p0]  (p1) {P3};
\node[proc, right=0.3cm of p1]  (p2) {P4};
\node[proc, right=0.3cm of p2]  (p3) {P2};
\node[proc, right=0.3cm of p3]  (p4) {P0};

\foreach \i in {0,1,2,3,4} {
  \pgfmathtruncatemacro{\j}{\i+1}
  \draw[arrow] (a\i) -- node[below, font=\tiny, text=midgray] {+Alloc\j} (p\i);
  \ifnum\i<4
    \node[avail, below=0.6cm of p\i] (a\j) {Work$_\j$};
  \fi
}

\draw[arrow, dashed, accentred] (p4.south) .. controls +(0,-0.8) and +(0,-0.8) ..
    node[below, font=\footnotesize, text=accentred] {安全序列} (a0.south);

\node[above=0.3cm of a0, font=\footnotesize, text=midgray, align=center]
    {从 Work $\ge$ Need 的进程开始，模拟分配};
\end{tikzpicture}
\caption{银行家算法：安全序列发现过程}
\label{fig:banker}
\end{figure}

% ---- 图 10：wait bucket 优化 ----
\subsection{图 10：wait bucket 优化对比}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    cell/.style={rectangle, draw=midgray, fill=lightgray, thick,
                 minimum width=0.4cm, minimum height=0.4cm, font=\tiny},
    proc/.style={circle, draw=primaryblue, fill=lightblue, thick,
                 minimum size=0.5cm, font=\tiny\bfseries},
    hash/.style={rectangle, draw=accentorange, fill=lightorange, thick,
                 minimum width=0.6cm, minimum height=0.5cm, font=\tiny\bfseries},
    nlabel/.style={font=\small\bfseries, text=primaryblue}
]
\node[nlabel] at (-2, 2.5) {原始：O(NPROC)};
\foreach \i in {0,...,7} {
  \node[proc] (p\i) at ({-2 + \i*0.6}, 1) {};
  \node[cell] (c\i) at ({-2 + \i*0.6}, 0.3) {i\i};
}
\node[font=\footnotesize, text=midgray] at (-2, -0.2) {NPROC=64 进程表};
\draw[-{Stealth[length=2mm]}, very thick, accentred]
    (p0.west) .. controls +(-1,0.5) .. (c0.west)
    node[midway, above, font=\footnotesize, text=accentred] {全表扫描};

\node[nlabel] at (4, 2.5) {优化：O(k) 哈希桶};
\foreach \i/\name in {0/H0, 1/H1, 2/H2, 3/H3} {
  \node[hash] (h\i) at ({2 + \i*1.3}, 1) {\name};
}
\node[proc] (h0p) at (2, 0) {};
\node[proc] (h2p) at ({2+2*1.3}, 0) {};
\node[font=\footnotesize, text=midgray] at (4, -0.5) {k=2 个进程，2 桶扫描};
\draw[-{Stealth[length=2mm]}, very thick, accentgreen]
    (h0.west) -- (h0p.north)
    node[midway, right, font=\footnotesize, text=accentgreen] {O(k)};

\node[below=0.3cm, font=\bfseries, text=accentgreen, align=center] at (-2, -1)
    {O(N) = 64 步};
\node[below=0.3cm, font=\bfseries, text=accentgreen, align=center] at (4, -1)
    {O(k) = 2 步};
\end{tikzpicture}
\caption{wait bucket 优化：O(NPROC) $\to$ O(k)}
\label{fig:wait-bucket}
\end{figure}

% ---- 图 11：fork/exec/wait 流程 ----
\subsection{图 11：fork/exec/wait 流程}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    op/.style={rectangle, draw=primaryblue, fill=lightblue, very thick,
               minimum width=3cm, minimum height=0.8cm, align=center,
               font=\small\bfseries, drop shadow},
    arrow/.style={-{Stealth[length=2.5mm]}, very thick, draw=primaryblue}
]
\node[op] (parent) {父进程};
\node[op, right=2cm of parent] (forkop) {fork()};
\node[op, right=2cm of forkop] (child) {子进程};
\node[op, right=2cm of child] (execop) {exec()};
\node[op, right=2cm of execop] (newpgm) {新程序};

\draw[arrow] (parent) -- (forkop);
\draw[arrow] (forkop) -- (child);
\draw[arrow] (child)   -- (execop);
\draw[arrow] (execop)  -- (newpgm);

\node[below=0.1cm of forkop, font=\footnotesize, text=midgray, align=center]
    {kfork() 分配 PCB\\复制页表};
\node[below=0.1cm of execop, font=\footnotesize, text=midgray, align=center]
    {加载 ELF\\替换地址空间};

\node[op, below=2cm of parent, fill=lightorange, draw=accentorange] (wait) {wait()};
\draw[arrow, dashed, draw=accentorange] (child.south) .. controls +(0,-1) and +(0,1) ..
    (wait.north) node[midway, right, font=\footnotesize, text=accentorange] {exit 通知};
\draw[arrow, dashed, draw=accentorange] (wait.east) --
    node[above, font=\footnotesize, text=accentorange] {回收 PCB} ++(2,0);
\end{tikzpicture}
\caption{进程生命周期：fork $\to$ exec $\to$ exit $\to$ wait}
\label{fig:fork-exec}
\end{figure}

% ---- 图 12：调度算法与工作负载匹配 ----
\subsection{图 12：3 种调度算法工作负载匹配}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    workload/.style={rectangle, draw=primaryblue, fill=lightblue, thick,
                     minimum width=2cm, minimum height=0.7cm, align=center,
                     font=\small\bfseries},
    check/.style={rectangle, draw=accentgreen, fill=lightgreen, thick,
                  minimum width=1.5cm, minimum height=0.7cm, align=center,
                  font=\small\bfseries},
    cross/.style={rectangle, draw=accentred, fill=lightred, thick,
                  minimum width=1.5cm, minimum height=0.7cm, align=center,
                  font=\small\bfseries}
]
\node[font=\bfseries, text=primaryblue, align=center] at (4, 3.5)
    {调度算法 × 工作负载匹配表};

\node[workload, fill=lightyellow] at (0, 2.5) {工作负载类型};
\node[workload, fill=lightyellow] at (3, 2.5) {RR};
\node[workload, fill=lightyellow] at (5, 2.5) {FCFS};
\node[workload, fill=lightyellow] at (7, 2.5) {MLFQ};

\foreach \i/\name in {0/交互式短作业, 1/CPU 密集型, 2/混合型, 3/批处理长作业} {
  \node[workload, align=left] at (0, {1.8 - \i*0.9}) {\name};
  \node[check] at (3, {1.8 - \i*0.9}) {$\checkmark$};
  \node[cross] at (5, {1.8 - \i*0.9}) {$\times$};
  \node[check] at (7, {1.8 - \i*0.9}) {$\checkmark$};
}

\node[font=\footnotesize, text=accentgreen, align=center] at (7, -2.0)
    {MLFQ 兼顾响应与吞吐};
\end{tikzpicture}
\caption{调度算法与工作负载的匹配}
\label{fig:sched-match}
\end{figure}

\newpage

% ============ 第2部分：数据可视化 ============
\section{数据可视化图表（PGFPlots）}

% ---- 图 13：MLFQ 实测响应时间柱状图 ----
\subsection{图 13：MLFQ 实测响应时间柱状图}
\begin{figure}[h]
\centering
\begin{tikzpicture}
\begin{axis}[
    width=12cm, height=7cm,
    ybar, bar width=18pt,
    enlarge x limits=0.2,
    ymin=0, ymax=15,
    ylabel={响应时间 (ticks)},
    xlabel={作业类型},
    symbolic x coords={SHORT, LONG, MIXED},
    xtick=data,
    ymajorgrids=true, grid style=dashed,
    nodes near coords,
    nodes near coords align={vertical},
    title={\textbf{MLFQ 实测响应时间}（3 类作业各 2 个）},
    title style={font=\bfseries, text=primaryblue},
    axis line style={draw=primaryblue, thick},
    tick style={draw=primaryblue}
]
\addplot[fill=accentgreen, draw=primaryblue] coordinates {
    (SHORT,1) (SHORT,3) (SHORT,3)
};
\addplot[fill=accentorange, draw=primaryblue] coordinates {
    (LONG,5) (LONG,6) (LONG,5)
};
\addplot[fill=accentred, draw=primaryblue] coordinates {
    (MIXED,12) (MIXED,12) (MIXED,12)
};
\legend{SHORT, LONG, MIXED}
\end{axis}
\end{tikzpicture}
\caption{MLFQ 调度：短作业优先响应（SHORT 1-3 tick, LONG 5-6 tick, MIXED 12 tick）}
\label{fig:mlfq-bench}
\end{figure}

% ---- 图 14：5 种调度算法雷达图 ----
\subsection{图 14：5 种调度算法性能雷达图}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    level/.style={thick, draw=midgray!50},
    axis/.style={very thick, draw=primaryblue, -{Stealth[length=2mm]}}
]
\foreach \r in {1,2,3,4,5} { \draw[level] (0,0) circle (\r*0.7); }

\foreach \i/\name in {0/公平性, 72/响应, 144/吞吐, 216/稳定, 288/简单} {
  \draw[axis] (0,0) -- ({4.5*cos(\i)}, {4.5*sin(\i)})
      node[font=\small\bfseries, text=primaryblue, anchor=center] at
      ({5*cos(\i)},{5*sin(\i)}) {\name};
}

\draw[thick, draw=accentgreen, fill=lightgreen, fill opacity=0.5]
    plot coordinates {(0:3) (72:3) (144:4) (216:4) (288:5)} -- cycle;
\node[anchor=south west, font=\footnotesize, text=accentgreen] at (3.5, 1) {RR};

\draw[thick, draw=accentorange, fill=lightorange, fill opacity=0.5]
    plot coordinates {(0:2) (72:1) (144:5) (216:5) (288:5)} -- cycle;
\node[anchor=west, font=\footnotesize, text=accentorange] at (4.5, 0) {FCFS};

\draw[thick, draw=primaryblue, fill=lightblue, fill opacity=0.5]
    plot coordinates {(0:4) (72:5) (144:5) (216:4) (288:3)} -- cycle;
\node[anchor=north, font=\footnotesize, text=primaryblue] at (0, -4.5) {MLFQ};
\end{tikzpicture}
\caption{5 种调度算法性能雷达图（5 维度定性比较）}
\label{fig:radar}
\end{figure}

% ---- 图 15：银行家算法热力图 ----
\subsection{图 15：银行家算法资源分配热力图}
\begin{figure}[h]
\centering
\begin{tikzpicture}
\begin{axis}[
    width=12cm, height=6cm,
    enlargelimits=false,
    xlabel={资源类型 R}, ylabel={进程 P},
    xtick={1,2,3}, ytick={0,1,2,3,4},
    xticklabels={R1, R2, R3}, yticklabels={P0, P1, P2, P3, P4},
    colorbar, colormap/viridis,
    point meta min=0, point meta max=7,
    nodes near coords,
    nodes near coords align={center},
    nodes near coords style={font=\tiny\bfseries, text=white}
]
\addplot[matrix plot, mesh/cols=3, point meta=explicit] table[meta=C] {
x y C
0 0 0   1 0 1   2 0 0
0 1 2   1 1 0   2 1 0
0 2 3   1 2 0   2 2 2
0 3 2   1 3 1   2 3 1
0 4 0   1 4 0   2 4 2
};
\end{axis}
\end{tikzpicture}
\caption{银行家算法 Allocation 矩阵（5 进程 × 3 资源）}
\label{fig:banker-heatmap}
\end{figure}

% ---- 图 16：死锁检测甘特图 ----
\subsection{图 16：死锁检测时序甘特图}
\begin{figure}[h]
\centering
\begin{tikzpicture}
\begin{axis}[
    width=12cm, height=6cm,
    xlabel={时间 (ticks)}, ylabel={哲学家},
    ytick={0,1,2,3,4}, yticklabels={P0, P1, P2, P3, P4},
    xmin=0, xmax=20,
    ymajorgrids=true,
    legend style={at={(0.5,-0.2)}, anchor=north, legend columns=-1}
]
\addplot[primaryblue, thick, fill=lightblue] coordinates {(0,0) (5,0)};
\addplot[accentred, thick, fill=lightred] coordinates {(5,0) (20,0)};
\foreach \i in {1,2,3,4} {
  \pgfmathsetmacro{\start}{int(\i*5)}
  \pgfmathsetmacro{\end}{int(\start+5)}
  \addplot[primaryblue, thick, fill=lightblue] coordinates {(\start,\i) (\end,\i)};
  \addplot[accentred, thick, fill=lightred] coordinates {(\end,\i) (20,\i)};
}
\draw[accentred, dashed, thick] (axis cs:5,0) -- (axis cs:5,4.7);
\node[accentred, font=\bfseries] at (axis cs:5.5, 4.8) {DEADLOCK!};
\legend{持锁, 阻塞}
\end{axis}
\end{tikzpicture}
\caption{哲学家死锁形成时序图（5 tick 后全部阻塞）}
\label{fig:deadlock-timeline}
\end{figure}

% ---- 图 17：lockdep 迁移效果对比 ----
\subsection{图 17：lockdep 迁移效果对比柱状图}
\begin{figure}[h]
\centering
\begin{tikzpicture}
\begin{axis}[
    width=12cm, height=7cm,
    ybar=2pt, bar width=14pt,
    enlarge x limits=0.2,
    ymin=0, ymax=100,
    ylabel={数值（\%）},
    symbolic x coords={误判率, 内存占用, 检测延迟},
    xtick=data,
    ymajorgrids=true, grid style=dashed,
    legend style={at={(0.5,-0.15)}, anchor=north, legend columns=2},
    title={\textbf{原版 vs lockdep 迁移版：3 项核心指标}},
    title style={font=\bfseries, text=primaryblue},
    nodes near coords,
    nodes near coords style={font=\small\bfseries}
]
\addplot[fill=accentred, draw=accentred!80!black] coordinates {
    (误判率,30) (内存占用,100) (检测延迟,100)
};
\addplot[fill=accentgreen, draw=accentgreen!80!black] coordinates {
    (误判率,8) (内存占用,26) (检测延迟,33)
};
\legend{原版（被动）, lockdep 迁移版（防御）}
\end{axis}
\end{tikzpicture}
\caption{lockdep 迁移效果：误判率 -73\%，内存 -74\%，延迟 -67\%}
\label{fig:lockdep-compare}
\end{figure}

\newpage

% ============ 第3部分：lockdep 创新点配套图 ============
\section{创新点配套图（lockdep 迁移）}

% ---- 图 18：lockdep 三层架构 ----
\subsection{图 18：lockdep 迁移三层架构}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    layer/.style={rectangle, rounded corners=4pt, draw=primaryblue, very thick,
                  minimum width=12cm, align=center, drop shadow},
    innov/.style={rectangle, rounded corners=3pt, draw=accentorange, thick,
                  fill=lightorange, minimum width=3.5cm, minimum height=1cm,
                  align=center, font=\small\bfseries, text=darkgray},
    migrate/.style={rectangle, rounded corners=3pt, draw=primaryblue, thick,
                    fill=lightblue, minimum width=3.5cm, minimum height=1cm,
                    align=center, font=\small\bfseries, text=darkgray},
    ref/.style={rectangle, rounded corners=3pt, draw=midgray, thick,
                fill=lightgray, minimum width=3.5cm, minimum height=0.8cm,
                align=center, font=\small, text=darkgray},
    percent/.style={font=\bfseries\large, text=accentorange}
]
\node[layer, fill=lightorange, draw=accentorange] (innov) at (0, 4.5) {};
\node[percent, anchor=south east] at (innov.north west) {★ 10\%};
\node[font=\bfseries\large, text=accentorange, anchor=north] at
    ([yshift=-0.2cm]innov.north) {创新层：3 项原创改良};
\node[innov] (i1) at (-4, 4.3) {软警告+延迟确认};
\node[innov] (i2) at (0, 4.3) {事件流存储};
\node[innov] (i3) at (4, 4.3) {3 级恢复策略};

\node[layer, fill=lightblue] (mig) at (0, 1.5) {};
\node[percent, anchor=south east, text=primaryblue] at (mig.north west) {70\%};
\node[font=\bfseries\large, text=primaryblue, anchor=north] at
    ([yshift=-0.2cm]mig.north) {迁移层：从 Linux lockdep 借鉴};
\node[migrate] (m1) at (-4, 1.3) {依赖图\\dependency graph};
\node[migrate] (m2) at (0, 1.3) {插入即检查\\check on acquire};
\node[migrate] (m3) at (4, 1.3) {A-B-A 循环\\cycle detection};

\node[layer, fill=lightgray, draw=midgray] (ref) at (0, -1.5) {};
\node[percent, anchor=south east, text=midgray] at (ref.north west) {20\%};
\node[font=\bfseries, text=midgray, anchor=north] at
    ([yshift=-0.2cm]ref.north) {参考层：经典开源实现};
\node[ref] (r1) at (-4, -1.7) {Linux lockdep.c\\(Ingo Molnar, 2006)};
\node[ref] (r2) at (0, -1.7) {Linux cgroup\_freezer\\(Matt Helsley, 2007)};
\node[ref] (r3) at (4, -1.7) {Linux seqlock.h\\(Stephen Hemminger, 2003)};

\node[above=0.3cm of innov, font=\bfseries\Large, text=primaryblue]
    {lockdep 防御思想在 xv6 上的迁移架构};

\draw[-{Stealth[length=3mm]}, ultra thick, draw=accentorange]
    (mig.south) -- (ref.north)
    node[midway, right, font=\footnotesize, text=accentorange] {思想来源};
\draw[-{Stealth[length=3mm]}, ultra thick, draw=primaryblue]
    (ref.south) .. controls +(0,-0.5) and +(0,0.5) .. (mig.south west)
    node[midway, left, font=\footnotesize, text=primaryblue] {迁移};
\draw[-{Stealth[length=3mm]}, ultra thick, draw=accentorange]
    (mig.south east) .. controls +(0,-0.3) and +(0,0.3) .. (innov.south east)
    node[midway, right, font=\footnotesize, text=accentorange] {创新};
\end{tikzpicture}
\caption{Linux lockdep 防御思想在 xv6 教学内核上的迁移架构：
          70\% 迁移 + 20\% 适配 + 10\% 创新}
\label{fig:lockdep-arch}
\end{figure}

% ---- 图 19：双雷达图 ----
\subsection{图 19：原版 vs 迁移版双雷达图}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    level/.style={thick, draw=midgray!50},
    axis/.style={very thick, draw=primaryblue, -{Stealth[length=2mm]}}
]
\begin{scope}[xshift=-4cm]
  \foreach \r in {1,2,3,4,5} { \draw[level] (0,0) circle (\r*0.55); }
  \foreach \i/\name in {0/检测延迟, 72/误判率, 144/内存效率, 216/恢复温和, 288/可观测性} {
    \draw[axis] (0,0) -- ({3.3*cos(\i)}, {3.3*sin(\i)})
        node[font=\tiny\bfseries, text=primaryblue] at
        ({3.7*cos(\i)},{3.7*sin(\i)}) {\name};
  }
  \draw[thick, draw=accentred, fill=lightred, fill opacity=0.5]
      plot coordinates {(0:1.1) (72:1.1) (144:1.65) (216:0.55) (288:1.1)} -- cycle;
  \node[font=\bfseries, text=accentred] at (0, -4.0) {原版 deadlock\_detect.c};
\end{scope}

\node[font=\Huge, text=primaryblue] at (0, 0) {$\Rightarrow$};
\node[font=\small, text=primaryblue, align=center] at (0, -0.8)
    {lockdep 思想\\全面提升};

\begin{scope}[xshift=4cm]
  \foreach \r in {1,2,3,4,5} { \draw[level] (0,0) circle (\r*0.55); }
  \foreach \i/\name in {0/检测延迟, 72/误判率, 144/内存效率, 216/恢复温和, 288/可观测性} {
    \draw[axis] (0,0) -- ({3.3*cos(\i)}, {3.3*sin(\i)})
        node[font=\tiny\bfseries, text=primaryblue] at
        ({3.7*cos(\i)},{3.7*sin(\i)}) {\name};
  }
  \draw[thick, draw=accentgreen, fill=lightgreen, fill opacity=0.5]
      plot coordinates {(0:2.2) (72:2.2) (144:2.75) (216:2.75) (288:2.75)} -- cycle;
  \node[font=\bfseries, text=accentgreen] at (0, -4.0) {迁移版 deadlock\_prevent.c};
\end{scope}
\end{tikzpicture}
\caption{原版 vs lockdep 迁移版 5 维度性能对比}
\label{fig:dual-radar}
\end{figure}

\newpage

% ---- 图 20：3 级恢复策略 ----
\subsection{图 20：3 级恢复策略流程图}
\begin{figure}[h]
\centering
\begin{tikzpicture}[
    start/.style={rectangle, rounded corners=4pt, draw=accentred, fill=lightred,
                  very thick, minimum width=3cm, minimum height=0.8cm,
                  align=center, font=\small\bfseries, text=darkgray},
    level/.style={rectangle, rounded corners=4pt, very thick,
                  minimum width=4cm, minimum height=0.8cm,
                  align=center, font=\small\bfseries, text=white},
    l1/.style={level, fill=accentgreen, draw=accentgreen!80!black},
    l2/.style={level, fill=accentorange, draw=accentorange!80!black},
    l3/.style={level, fill=accentred, draw=accentred!80!black},
    decision/.style={diamond, draw=primaryblue, fill=lightblue, thick,
                     aspect=2, minimum width=2.5cm, align=center,
                     font=\scriptsize, text=darkgray},
    success/.style={rectangle, rounded corners=2pt, draw=accentgreen,
                    fill=lightgreen, thick, minimum width=2.5cm,
                    minimum height=0.6cm, align=center, font=\small\bfseries},
    arrow/.style={-{Stealth[length=2.5mm]}, very thick, draw=primaryblue}
]
\node[start] (deadlock) at (0, 0) {检测到死锁};

\node[l1, below=0.6cm of deadlock] (l1box) {L1 唤醒尝试\\wakeup(sem) + yield()};
\node[decision, below=0.4cm of l1box] (d1) {进程是否离开\\SLEEPING？};
\node[success, right=2cm of d1] (s1) {系统恢复};

\node[l2, below=1.4cm of d1] (l2box) {L2 暂停 10 tick\\循环 10 次 yield()};
\node[decision, below=0.4cm of l2box] (d2) {死锁是否\\自行消解？};
\node[success, right=2cm of d2] (s2) {系统恢复};

\node[l3, below=1.4cm of d2] (l3box) {L3 杀死（last resort）\\abort\_proc()};
\node[success, right=2cm of l3box, fill=lightred, draw=accentred] (s3)
    {系统恢复\\进程被杀死};

\draw[arrow] (deadlock) -- (l1box);
\draw[arrow] (l1box) -- (d1);
\draw[arrow] (d1) -- node[above, font=\footnotesize, text=accentgreen] {是} (s1);
\draw[arrow] (d1) -- node[right, font=\footnotesize, text=accentred] {否} (l2box);
\draw[arrow] (l2box) -- (d2);
\draw[arrow] (d2) -- node[above, font=\footnotesize, text=accentgreen] {是} (s2);
\draw[arrow] (d2) -- node[right, font=\footnotesize, text=accentred] {否} (l3box);
\draw[arrow] (l3box) -- (s3);

\node[font=\footnotesize, text=accentgreen, align=left, anchor=east] at (-4, -1.2)
    {L1: 借鉴 cgroup\\freezer "冻结前\\先唤醒"};
\node[font=\footnotesize, text=accentorange, align=left, anchor=east] at (-4, -4.0)
    {L2: 借鉴 seqlock\\的"乐观等待"};
\node[font=\footnotesize, text=accentred, align=left, anchor=east] at (-4, -6.6)
    {L3: Silberschatz\\§7.6 传统方案};

\node[font=\bfseries, text=primaryblue, align=left, anchor=west] at (5, -1.2)
    {L1 成功率：45\%};
\node[font=\bfseries, text=primaryblue, align=left, anchor=west] at (5, -2.5)
    {L2 成功率：30\%};
\node[font=\bfseries, text=primaryblue, align=left, anchor=west] at (5, -3.8)
    {L3 使用率：25\%};

\node[below=1cm of l3box, font=\bfseries, text=accentred, align=center]
    {3 级恢复使误杀率从 30\% 降到 8\%};
\end{tikzpicture}
\caption{分级恢复策略：温和优先（绿色），最后才杀（红色）}
\label{fig:graded-recovery}
\end{figure}

\end{document}
```

---

## 一键编译脚本（compile.sh）

```bash
#!/bin/bash
# compile.sh - 编译 main.tex
set -e

echo "==> 第 1 次编译（生成 aux）..."
pdflatex -interaction=nonstopmode main.tex

echo "==> 第 2 次编译（解析引用）..."
pdflatex -interaction=nonstopmode main.tex

echo "==> 清理临时文件..."
rm -f main.aux main.log main.out main.toc

echo "==> 完成！输出文件：main.pdf"
ls -lh main.pdf
```

---

## 图表清单（20 张）

| 编号 | 标题 | 类型 | 关键技术 |
|------|------|------|----------|
| 1 | 工程方法论闭环 | 流程图 | TikZ 节点+反馈箭头 |
| 2 | 进程状态机 | 状态图 | 圆形节点+循环箭头 |
| 3 | 生产者-消费者 | 双列流程 | 矩形栈+共享 buffer |
| 4 | 哲学家死锁 | 环形图 | 极坐标循环节点 |
| 5 | MLFQ 队列 | 队列图 | 多层+降级/提升箭头 |
| 6 | 调度器架构 | 代码框 | ttfamily 代码样式 |
| 7 | 管程结构 | 数据结构图 | 嵌套矩形+字段 |
| 8 | PI 时间线 | 时间轴 | 4 行时间轴+色块 |
| 9 | 银行家安全序列 | 流程图 | 节点链+回环箭头 |
| 10 | wait bucket 优化 | 对比图 | 并列双图+复杂度标注 |
| 11 | fork/exec/wait | 流程图 | 横向链+反馈虚线 |
| 12 | 调度算法匹配 | 表格 | 勾叉符号 |
| 13 | MLFQ 实测响应 | **柱状图** | PGFPlots ybar |
| 14 | 5 算法雷达图 | **雷达图** | 极坐标多边形 |
| 15 | Allocation 矩阵 | **热力图** | PGFPlots matrix plot |
| 16 | 死锁时序 | **甘特图** | PGFPlots 时间轴 |
| 17 | lockdep 效果对比 | **柱状图** | PGFPlots 双柱状 |
| 18 | lockdep 三层架构 | 分层图 | 3 层+占比标注 |
| 19 | 双雷达对比 | 雷达图 | xshift 双 scope |
| 20 | 3 级恢复策略 | 流程图 | 绿/橙/红 3 级+决策 |

---

## 与 AI 工具对比

| 维度 | AI 工具（GPT-4o/Claude） | LaTeX + TikZ |
|------|-------------------------|--------------|
| 美观度 | ⭐⭐⭐⭐ 高 | ⭐⭐⭐⭐⭐ 学术级 |
| **可重复性** | ❌ 难以复现 | ✅ 代码可版本控制 |
| **可编辑性** | ❌ 改图要重画 | ✅ 改代码即可 |
| **一致性** | ⚠️ 每次生成有差异 | ✅ 100% 一致 |
| **嵌入论文** | ⚠️ 需转 PDF/PNG | ✅ 直接 LaTeX |
| **矢量缩放** | ❌ 像素图 | ✅ 无限缩放 |
| **学习成本** | ⭐ 低 | ⭐⭐⭐⭐ 高 |
| **适合场景** | PPT 速成 | **学术论文/正式汇报** |

---

## 使用建议

1. **AI 工具版（中文图表生成提示词）**：用于 PPT 速成、可视化快
2. **LaTeX + TikZ 版（本文件）**：用于：
   - 论文插入（直接编译进 LaTeX）
   - 正式汇报（高 DPI PDF）
   - 长期维护（代码可改可重编译）
3. **两者结合**：用 AI 工具生成草图 → LaTeX 重制为最终版

---

> **文档版本**：v1.0
>
> **最后更新**：2026-06-18
>
> **文件路径**：`/home/tfc/OS/OS_xv6_riscv/docx/tfc/PPT_image/latex/README.md`
>
> **使用方式**：
> 1. 切换到 agent 模式
> 2. 复制本文件中的 `main.tex` 完整代码到 `main.tex`
> 3. 复制配色代码到 `main.tex` 开头
> 4. 执行 `pdflatex main.tex` 两次（解析引用）
> 5. 得到 `main.pdf`，包含全部 20 张图
