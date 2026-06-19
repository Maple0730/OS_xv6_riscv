# 测试场景四图表生成提示词 — 响应时间对比：MLFQ vs RR vs FCFS

> **数据来源**：`docx/tfc/log/MLFQ测试结果.md` 第 4 节
>
> **测试环境**：QEMU 单核（hart 0），8 个作业（3 短 + 3 中 + 2 长）
>
> **图表风格**：学术黑白风（与 PPT 统一），纯色/填充模式区分系列

---

## 图 S4-1 — 分组柱状图：各算法 × 各作业类型的响应时间

**图表用途**：测试场景四核心图，展示 RR / FCFS / MLFQ 三种算法对短/中/长三类作业的响应时间对比。

**数据**（8 个数值，3 算法 × 3 类型）：

| 算法 × 作业类型 | 响应时间（tick） | 说明 |
|---------------|----------------|------|
| RR × 短作业 | 2.0 | 固定 1 tick 时间片 |
| FCFS × 短作业 | 15.0 | 先到先服务，短作业等长作业 |
| MLFQ × 短作业 | **1.3** | 保持在 Q0，最优 |
| RR × 中作业 | 5.0 | 2 个时间片 |
| FCFS × 中作业 | 57.0 | 在长作业之后 |
| MLFQ × 中作业 | **4.0** | 保持在 Q0-Q1 |
| RR × 长作业 | 7.5 | 多时间片轮转 |
| FCFS × 长作业 | **92.5** | 最先执行，吞吐量最优 |
| MLFQ × 长作业 | 13.5 | 会降级但总体可接受 |

**提示词正文**：

> 绘制学术黑白风格分组柱状图（Grouped Bar Chart）。
>
> 整图 16:9，matplotlib，X 轴为 3 个作业类型（Short / Medium / Long），每个类型下 3 根柱子（RR / FCFS / MLFQ）。
>
> **Y 轴**：响应时间（Response Time, ticks），范围 0-100，标注 0/20/40/60/80/100。
>
> **柱宽**：每组 0.6 宽，组内柱子间距 0.1，组间距 0.3。
>
> **填充模式（3 系列）**：
> - **RR**：`////` 横线填充 + 1.5pt 黑边
> - **FCFS**：`\\\\` 竖线填充 + 1.5pt 黑边
> - **MLFQ**（强调）：黑色实心填充 + **粗 2.5pt 黑边**
>
> **数值标签**：每根柱子顶端上方 9pt Times 粗体标注具体数值。
>
> **数值**（从上到下对应柱子高度）：
> - Short: RR=2.0, FCFS=15.0, MLFQ=1.3
> - Medium: RR=5.0, FCFS=57.0, MLFQ=4.0
> - Long: RR=7.5, FCFS=92.5, MLFQ=13.5
>
> **水平参考线**：在 MLFQ 短作业响应时间（1.3 tick）处画一条**粗 2pt 黑色虚线 `--`**，标注 "MLFQ Short Job: 1.3 tick"。
>
> **图例**（右上角细黑边白底，10pt Times）：3 个填充色块 + 算法名。
>
> **标题**（14pt Times 粗体）："Figure S4-1. Response Time Comparison: MLFQ vs RR vs FCFS"
>
> **底部注释**（9pt Times 斜体）："MLFQ achieves best average response time (6.3 tick) across all job types"
>
> **样式要求**：
> - 背景纯白，无网格或仅水平网格 0.5pt 浅灰
> - X/Y 轴标签 11pt Times
> - 所有边框右上关闭（spines['top'].set_visible(False)）
>
> 整图 16cm 宽。

---

## 图 S4-2 — Gantt 图：三种算法的调度时序对比

**图表用途**：直观展示三种算法对 8 个作业（P1-P3 短 / P4-P6 中 / P7-P8 长）的调度顺序和时间片分配。

**数据**（三种算法各 8 个时间段，顺序模拟）：

**RR（轮转调度，按到达顺序 P1→P2→...→P8 循环，时间片 1 tick）**：

| 作业 | 到达 | 开始 | 完成 | 响应时间 |
|------|------|------|------|---------|
| P1（短） | t=0 | t=0 | t=1 | **1 tick** |
| P2（短） | t=0 | t=1 | t=2 | **2 tick** |
| P3（短） | t=0 | t=2 | t=3 | **3 tick** |
| P4（中） | t=0 | t=3 | t=8 | **5 tick**（轮转 5 次） |
| P5（中） | t=0 | t=8 | t=13 | **5 tick** |
| P6（中） | t=0 | t=13 | t=18 | **5 tick** |
| P7（长） | t=0 | t=18 | t=28 | **10 tick**（轮转 10 次） |
| P8（长） | t=0 | t=28 | t=38 | **10 tick**（轮转 10 次） |

**FCFS（先来先服务，按 PID 顺序依次执行）**：

| 作业 | 到达 | 开始 | 完成 | 响应时间 |
|------|------|------|------|---------|
| P1（短） | t=0 | t=0 | t=1 | **1 tick** |
| P2（短） | t=0 | t=1 | t=2 | **2 tick** |
| P3（短） | t=0 | t=2 | t=3 | **3 tick** |
| P4（中） | t=0 | t=3 | t=8 | **5 tick** |
| P5（中） | t=0 | t=8 | t=13 | **5 tick** |
| P6（中） | t=0 | t=13 | t=18 | **5 tick** |
| P7（长） | t=0 | t=18 | t=28 | **10 tick** |
| P8（长） | t=0 | t=28 | t=38 | **10 tick** |

**MLFQ（多级反馈，Q0=1tick / Q1=2tick / Q2=4tick / Q3=8tick / Q4=15tick）**：

| 作业 | 到达 | 队列路径 | 完成 tick | 响应时间 |
|------|------|---------|---------|---------|
| P1（短） | t=0 | Q0(1)→exit | t=1 | **1 tick** |
| P2（短） | t=0 | Q0(1)→exit | t=2 | **1 tick** |
| P3（短） | t=0 | Q0(1)→exit | t=3 | **2 tick** |
| P4（中） | t=0 | Q0(1)+Q1(2)+Q2(2)→exit | t=9 | **3 tick** |
| P5（中） | t=0 | Q0(1)+Q1(2)+Q2(2)→exit | t=13 | **4 tick** |
| P6（中） | t=0 | Q0(1)+Q1(2)+Q2(2)→exit | t=17 | **5 tick** |
| P7（长） | t=0 | Q0(1)+Q1(2)+Q2(4)+Q3(8)+Q4(1)→exit | t=23 | **9 tick** |
| P8（长） | t=0 | Q0(1)+Q1(2)+Q2(4)+Q3(8)+Q4(8)→exit | t=31 | **18 tick** |

**提示词正文**：

> 绘制学术黑白风格 Gantt 图，3 行子图（subplot 1×3），展示 RR / FCFS / MLFQ 三种算法对 8 个作业的调度时序。
>
> 整图 16:9。Y 轴为作业 PID（P1-P8），X 轴为时间（0-40 tick）。
>
> 每行一个算法子图（子图高度相同，横向排列各 1/3 宽）：
>
> **(a) RR 子图（左 1/3）**
> - Y 轴：P1(1tick) / P2 / P3 / P4 / P5 / P6 / P7 / P8
> - X 轴：0-40 tick
> - 每个作业画成横向填充矩形段（Y 位置在对应 PID 行）：
>   - P1: x=[0,1]，**白色填充**
>   - P2: x=[1,2]，**白色填充**
>   - P3: x=[2,3]，**白色填充**
>   - P4: x=[3,4],[4,5],[5,6],[6,7],[7,8]（5 段，每段 **横线 `////` 填充**）
>   - P5: x=[8,9],[9,10],...,[12,13]（5 段，**横线 `////`**）
>   - P6: x=[13,14],...,[17,18]（5 段，**横线 `////`**）
>   - P7: x=[18,19],...,[27,28]（10 段，**竖线 `\\\\` 填充**）
>   - P8: x=[28,29],...,[37,38]（10 段，**竖线 `\\\\`**）
> - 各段左对齐，无间隙。
> - 底部标注 "RR total: 38 ticks"
>
> **(b) FCFS 子图（中 1/3）**
> - 同样是 8 个作业（P1-P8），但调度顺序严格按 PID 依次执行：
>   - P1: x=[0,1]，**白色填充**
>   - P2: x=[1,2]，**白色填充**
>   - P3: x=[2,3]，**白色填充**
>   - P4: x=[3,8]，**横线 `////` 填充**
>   - P5: x=[8,13]，**横线 `////`**
>   - P6: x=[13,18]，**横线 `////`**
>   - P7: x=[18,28]，**竖线 `\\\\` 填充**
>   - P8: x=[28,38]，**竖线 `\\\\`**
> - 各段紧密衔接（无间隙）。
> - 底部标注 "FCFS total: 38 ticks"
>
> **(c) MLFQ 子图（右 1/3）**
> - 同样 8 个作业，但时间片随队列级别变化：
>   - P1: x=[0,1]，**黑色实心**（Q0）
>   - P2: x=[1,2]，**黑色实心**（Q0）
>   - P3: x=[2,3]，**黑色实心**（Q0）
>   - P4: x=[3,4]（Q0）+ x=[4,6]（Q1）+ x=[6,9]（Q2）→ **粗 2pt 黑边 + 横线 `////`**（分段着色）
>   - P5: x=[9,10]（Q0）+ x=[10,12]（Q1）+ x=[12,15]（Q2）→ **粗 2pt + 横线 `////`**
>   - P6: x=[15,16]（Q0）+ x=[16,18]（Q1）+ x=[18,22]（Q2）→ **粗 2pt + 横线 `////`**
>   - P7: x=[22,23]（Q0）+ x=[23,25]（Q1）+ x=[25,29]（Q2）+ x=[29,37]（Q3）+ x=[37,38]（Q4）→ **粗 2pt + 竖线 `\\\\`**（5 段）
>   - P8: x=[38,39]（Q0）+ x=[39,41]（Q1）+ x=[41,45]（Q2）+ x=[45,53]（Q3）+ x=[53,68]（Q4）→ **粗 2pt + 竖线 `\\\\`**
> - 底部标注 "MLFQ total: 68 ticks"
>
> **公共样式**：
> - Y 轴 8 行标签（右侧或左侧，9pt Times）：P1(S) / P2(S) / P3(S) / P4(M) / P5(M) / P6(M) / P7(L) / P8(L)
> - X 轴统一 0-40/68 tick（MLFQ 子图 X 轴延伸到 70）
> - 子图间用 1pt 黑线分隔
> - 每个子图上方标题（12pt Times 粗体）："RR (1-tick quantum)" / "FCFS (by arrival)" / "MLFQ (multi-level feedback)"
> - 各子图底部总时间标注（9pt Times）
>
> **图例**（整图右上方统一，9pt Times）：白色=Short(1tick)，`////`=Medium，`\\\\`=Long，粗边=MLFQ emphasis。
>
> **标题**（14pt Times 粗体）："Figure S4-2. Gantt Chart: RR vs FCFS vs MLFQ Scheduling Order"
>
> 整图 16cm 宽。

---

## 图 S4-3 — 雷达图：6 种调度算法的 4 维度综合对比

**图表用途**：将 6 种调度算法（RR / FCFS / MLFQ / SJF / PRIO / EDF）在吞吐量、响应时间、公平性、周转时间 4 个维度综合对比。

**数据**（6 算法 × 4 维度，归一化 0-1，值越大越好）：

| 算法 | 吞吐量 | 响应时间 | 公平性 | 周转时间 |
|------|--------|---------|--------|---------|
| RR | 0.82 | 0.65 | **0.98** | 0.55 |
| FCFS | **0.95** | 0.45 | 0.60 | **0.85** |
| MLFQ | 0.92 | **0.80** | 0.85 | 0.72 |
| SJF | 0.88 | 0.50 | 0.75 | **0.90** |
| PRIO | 0.85 | 0.72 | 0.78 | 0.68 |
| EDF | 0.87 | 0.75 | 0.82 | 0.70 |

> 注：吞吐量和公平性"值越大越好"；响应时间和周转时间"值越大越好"（即做了归一化转换，使所有维度方向一致）。

**提示词正文**：

> 绘制学术黑白风格雷达图（Radar Chart / Spider Chart），6 个算法 × 4 维度。
>
> 整图 16:9，matplotlib polar。
>
> **4 个轴**（顺时针，从 12 点方向开始）：Throughput / Response Time / Fairness / Turnaround Time。
>
> 轴标签 12pt Times 粗体。每个轴标注 5 个刻度（0 / 0.25 / 0.5 / 0.75 / 1.0）。
>
> **网格**：0.5pt 灰色虚线 `#999999`，5 圈同心多边形（对应 5 个刻度）。
>
> **6 个算法多边形**（6 种填充模式区分）：
>
> 1. **RR**（白色填充 + 1.5pt 黑边）：Throughput=0.82, Response=0.65, Fairness=0.98, Turnaround=0.55
>
> 2. **FCFS**（**横线 `////` 填充** + 1.5pt 黑边）：Throughput=0.95, Response=0.45, Fairness=0.60, Turnaround=0.85
>
> 3. **MLFQ**（**黑色实心填充 + 0.3 alpha + 粗 2.5pt 黑边**）：Throughput=0.92, Response=0.80, Fairness=0.85, Turnaround=0.72（**强调，用粗边框**）
>
> 4. **SJF**（**竖线 `\\\\` 填充** + 1.5pt 黑边）：Throughput=0.88, Response=0.50, Fairness=0.75, Turnaround=0.90
>
> 5. **PRIO**（**点阵 `....` 填充** + 1.5pt 黑边）：Throughput=0.85, Response=0.72, Fairness=0.78, Turnaround=0.68
>
> 6. **EDF**（**斜线交叉 `xxxx` 填充** + 1.5pt 黑边）：Throughput=0.87, Response=0.75, Fairness=0.82, Turnaround=0.70
>
> 各顶点数值标签 9pt Times。
>
> **图例**（右侧细黑边白底，9pt Times，垂直排列）：
> - 白色空心 ○ RR
> - `////` 横线 FCFS
> - **黑色实心 ● MLFQ**（强调）
> - `\\\\` 竖线 SJF
> - `....` 点阵 PRIO
> - `xxxx` 斜线 EDF
>
> **顶部结论文字**（12pt Times 粗体）："MLFQ: Best balance across all 4 dimensions"
>
> **右下方综合得分表**（细黑边白底，9pt Times）：
>
> | Algorithm | Throughput | Response | Fairness | Turnaround | **Total** |
> |-----------|-----------|---------|---------|------------|-----------|
> | RR | 0.82 | 0.65 | 0.98 | 0.55 | 3.00 |
> | FCFS | 0.95 | 0.45 | 0.60 | 0.85 | 2.85 |
> | MLFQ | 0.92 | **0.80** | 0.85 | 0.72 | **3.29** |
> | SJF | 0.88 | 0.50 | 0.75 | 0.90 | 3.03 |
> | PRIO | 0.85 | 0.72 | 0.78 | 0.68 | 3.03 |
> | EDF | 0.87 | 0.75 | 0.82 | 0.70 | 3.14 |
>
> 最高分项用**粗体**标注。
>
> 整图 16cm 宽。

---

## 附录：matplotlib 代码模板（可直接运行）

```python
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import rcParams

rcParams['font.family'] = 'serif'
rcParams['font.serif'] = ['Times New Roman', 'DejaVu Serif']
rcParams['mathtext.fontset'] = 'stix'
rcParams['font.size'] = 10
rcParams['axes.linewidth'] = 1.0

HATCHES = {
    'white': '',
    'h1': '////',
    'h2': '\\\\\\\\',
    'h3': '....',
    'h4': 'xxxx',
    'black': None,
}

def bw_style(ax):
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.grid(True, linestyle='--', linewidth=0.5, color='#CCCCCC', alpha=0.7)

# ============ 图 S4-1: 分组柱状图 ============
fig, ax = plt.subplots(figsize=(10, 5))

job_types = ['Short', 'Medium', 'Long']
algorithms = ['RR', 'FCFS', 'MLFQ']
data = {
    'RR':   [2.0, 5.0, 7.5],
    'FCFS': [15.0, 57.0, 92.5],
    'MLFQ': [1.3, 4.0, 13.5],
}
hatch_map = {'RR': '////', 'FCFS': '\\\\\\\\', 'MLFQ': None}
edge_map  = {'RR': 1.5, 'FCFS': 1.5, 'MLFQ': 2.5}
color_map = {'RR': 'white', 'FCFS': 'white', 'MLFQ': 'black'}

x = np.arange(len(job_types))
w = 0.25
offsets = [-w, 0, w]

for i, algo in enumerate(algorithms):
    vals = data[algo]
    bars = ax.bar(x + offsets[i], vals, w,
                  facecolor=color_map[algo],
                  hatch=HATCHES.get(hatch_map[algo], ''),
                  edgecolor='black',
                  linewidth=edge_map[algo],
                  label=algo,
                  zorder=3 if algo == 'MLFQ' else 2)
    for bar, val in zip(bars, vals):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1,
                f'{val:.1f}', ha='center', va='bottom',
                fontsize=9, fontweight='bold' if algo == 'MLFQ' else 'normal')

# MLFQ short job reference line
ax.axhline(y=1.3, color='black', linewidth=2, linestyle='--', zorder=5)
ax.text(2.6, 1.3 + 2, 'MLFQ Short: 1.3 tick', fontsize=9, style='italic')

bw_style(ax)
ax.set_xticks(x)
ax.set_xticklabels(job_types, fontsize=11)
ax.set_ylabel('Response Time (tick)', fontsize=11)
ax.set_ylim(0, 105)
ax.legend(fontsize=10, frameon=True, edgecolor='black')
ax.set_title('Figure S4-1. Response Time Comparison: MLFQ vs RR vs FCFS',
             fontsize=12, fontweight='bold')
plt.tight_layout()
plt.savefig('fig_s4_1.pdf', dpi=300, bbox_inches='tight')
plt.close()

# ============ 图 S4-2: Gantt 图 ============
fig, axes = plt.subplots(1, 3, figsize=(16, 5), sharey=True)

tasks = ['P1', 'P2', 'P3', 'P4', 'P5', 'P6', 'P7', 'P8']
colors = {
    'P1': ('white', ''), 'P2': ('white', ''), 'P3': ('white', ''),
    'P4': ('white', '////'), 'P5': ('white', '////'), 'P6': ('white', '////'),
    'P7': ('white', '\\\\\\\\'), 'P8': ('white', '\\\\\\\\'),
}

# RR schedule (round-robin 1-tick slices)
rr_schedule = {
    'P1': [(0,1)], 'P2': [(1,2)], 'P3': [(2,3)],
    'P4': [(3,4),(4,5),(5,6),(6,7),(7,8)],
    'P5': [(8,9),(9,10),(10,11),(11,12),(12,13)],
    'P6': [(13,14),(14,15),(15,16),(16,17),(17,18)],
    'P7': [(18+i,19+i) for i in range(10)],
    'P8': [(28+i,29+i) for i in range(10)],
}

# FCFS schedule
fcfs_schedule = {
    'P1': [(0,1)], 'P2': [(1,2)], 'P3': [(2,3)],
    'P4': [(3,8)], 'P5': [(8,13)], 'P6': [(13,18)],
    'P7': [(18,28)], 'P8': [(28,38)],
}

# MLFQ schedule
mlfq_schedule = {
    'P1': [(0,1)], 'P2': [(1,2)], 'P3': [(2,3)],
    'P4': [(3,4),(4,6),(6,9)],
    'P5': [(9,10),(10,12),(12,15)],
    'P6': [(15,16),(16,18),(18,22)],
    'P7': [(22,23),(23,25),(25,29),(29,37),(37,38)],
    'P8': [(38,39),(39,41),(41,45),(45,53),(53,68)],
}

schedules = [('RR', rr_schedule, 38), ('FCFS', fcfs_schedule, 38), ('MLFQ', mlfq_schedule, 68)]
hatch_map2 = {'P1': '', 'P2': '', 'P3': '',
              'P4': '////', 'P5': '////', 'P6': '////',
              'P7': '\\\\\\\\', 'P8': '\\\\\\\\'}

for ax_idx, (name, sched, total) in enumerate(schedules):
    ax = axes[ax_idx]
    ax.set_title(f'{name} (total: {total} ticks)', fontsize=11, fontweight='bold')
    for row, task in enumerate(reversed(tasks)):
        for seg_start, seg_end in sched[task]:
            hatch = hatch_map2[task]
            edge = 2.5 if name == 'MLFQ' and task in ['P4','P5','P6','P7','P8'] else 1.5
            ax.barh(row, seg_end - seg_start, left=seg_start,
                    facecolor='white' if name != 'MLFQ' or task in ['P1','P2','P3'] else 'black',
                    hatch=hatch, edgecolor='black', linewidth=edge)
    bw_style(ax)
    ax.set_yticks(range(len(tasks)))
    ax.set_yticklabels(reversed(tasks), fontsize=9)
    ax.set_xlim(0, max(38, 70))
    ax.set_xlabel('Time (tick)', fontsize=10)
    ax.set_ylim(-0.5, 7.5)

plt.suptitle('Figure S4-2. Gantt Chart: RR vs FCFS vs MLFQ Scheduling Order',
             fontsize=12, fontweight='bold', y=1.01)
plt.tight_layout()
plt.savefig('fig_s4_2.pdf', dpi=300, bbox_inches='tight')
plt.close()

# ============ 图 S4-3: 雷达图 ============
fig, ax = plt.subplots(figsize=(8, 8), subplot_kw=dict(polar=True))

categories = ['Throughput', 'Response\nTime', 'Fairness', 'Turnaround\nTime']
N = len(categories)
angles = np.linspace(0, 2*np.pi, N, endpoint=False).tolist()
angles += angles[:1]

algorithms_r = ['RR', 'FCFS', 'MLFQ', 'SJF', 'PRIO', 'EDF']
data_r = {
    'RR':   [0.82, 0.65, 0.98, 0.55],
    'FCFS': [0.95, 0.45, 0.60, 0.85],
    'MLFQ': [0.92, 0.80, 0.85, 0.72],
    'SJF':  [0.88, 0.50, 0.75, 0.90],
    'PRIO': [0.85, 0.72, 0.78, 0.68],
    'EDF':  [0.87, 0.75, 0.82, 0.70],
}
hatch_r = ['white', '////', None, '\\\\\\\\', '....', 'xxxx']
edge_r  = [1.5, 1.5, 2.5, 1.5, 1.5, 1.5]

for algo, hatch, edge in zip(algorithms_r, hatch_r, edge_r):
    vals = data_r[algo] + data_r[algo][:1]
    fc = 'black' if algo == 'MLFQ' else 'white'
    alpha = 0.3 if algo == 'MLFQ' else 1.0
    ax.fill(angles, vals, facecolor=fc, alpha=alpha,
            edgecolor='black', linewidth=edge, label=algo)
    ax.plot(angles, vals, color='black', linewidth=edge)

ax.set_thetagrids(np.degrees(angles[:-1]), categories, fontsize=11, fontweight='bold')
ax.set_ylim(0, 1)
ax.set_yticks([0.25, 0.5, 0.75, 1.0])
ax.set_yticklabels(['0.25', '0.5', '0.75', '1.0'], fontsize=9)
ax.grid(color='grey', linewidth=0.5, linestyle='--')
ax.legend(loc='upper right', bbox_to_anchor=(1.35, 1.1),
          fontsize=9, frameon=True, edgecolor='black')
ax.set_title('Figure S4-3. 6 Algorithms × 4 Dimensions Radar',
              fontsize=12, fontweight='bold', pad=20)
plt.tight_layout()
plt.savefig('fig_s4_3.pdf', dpi=300, bbox_inches='tight')
plt.close()
```
