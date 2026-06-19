#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure S4-1: MLFQ vs RR vs FCFS Response Time Comparison
Academic black-and-white style, Chinese labels.
"""

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
import numpy as np

# ============ 字体配置 ============
# 查找 Noto Sans CJK SC 字体（简体中文）
chinese_fonts = [
    'Noto Sans CJK SC',
    'Noto Sans CJK TC',
    'Noto Sans CJK JP',
    'Noto Sans CJK HK',
    'AR PL UMing CN',
    'WenQuanYi Micro Hei',
]
font_path = None
for f in fm.fontManager.ttflist:
    if 'Noto Sans CJK SC' in f.name and 'Regular' in f.name:
        font_path = f.fname
        print(f"Found font: {f.name} at {f.fname}")
        break

if font_path is None:
    for f in fm.fontManager.ttflist:
        for cf in chinese_fonts:
            if cf in f.name:
                font_path = f.fname
                print(f"Found font: {f.name} at {f.fname}")
                break
        if font_path:
            break

if font_path:
    prop = fm.FontProperties(fname=font_path)
    plt.rcParams['font.family'] = prop.get_name()
else:
    print("WARNING: No Chinese font found!")

# ============ 全局样式 ============
plt.rcParams.update({
    'font.size': 10,
    'axes.linewidth': 1.2,
    'xtick.major.width': 0.8,
    'ytick.major.width': 0.8,
    'grid.linewidth': 0.5,
    'grid.alpha': 0.5,
    'axes.grid': True,
    'axes.grid.axis': 'y',
    'grid.color': '#999999',
    'grid.linestyle': '--',
    'axes.spines.top': False,
    'axes.spines.right': False,
})

# ============ 数据 ============
job_types = ['短作业\n(<1 tick)', '中等作业\n(≈5 tick)', '长作业\n(≈20 tick)']

data = {
    'MLFQ': [1.3, 4.0, 13.5],    # 黑色实心（强调）
    'RR':   [2.0, 5.0, 7.5],     # 横线填充
    'FCFS': [15.0, 57.0, 92.5],  # 竖线填充
}

# ============ 绘制 ============
fig, ax = plt.subplots(figsize=(11, 6.5))

n_groups = len(job_types)
bar_width = 0.22
group_spacing = 1.0
group_centers = np.arange(n_groups) * group_spacing
offsets = np.array([-bar_width, 0, bar_width])

# 颜色定义
HATCH_FCFS = '\\\\'   # 竖线
HATCH_RR   = '///'    # 横线

for i, (algo, values) in enumerate(data.items()):
    positions = group_centers + offsets[i]

    if algo == 'MLFQ':
        bars = ax.bar(positions, values, bar_width,
                      facecolor='black',
                      edgecolor='black',
                      linewidth=2.5,
                      label='MLFQ（黑色实心，强调）',
                      zorder=3)
    elif algo == 'RR':
        bars = ax.bar(positions, values, bar_width,
                      facecolor='white',
                      edgecolor='black',
                      linewidth=1.0,
                      hatch=HATCH_RR,
                      label='RR（横线，轮转调度）',
                      zorder=2)
    else:  # FCFS
        bars = ax.bar(positions, values, bar_width,
                      facecolor='white',
                      edgecolor='black',
                      linewidth=1.0,
                      hatch=HATCH_FCFS,
                      label='FCFS（竖线，先来先服务）',
                      zorder=2)

    # 数值标注
    for bar, val in zip(bars, values):
        ax.text(bar.get_x() + bar.get_width() / 2,
                bar.get_height() + 1.5,
                f'{val:.1f}',
                ha='center', va='bottom',
                fontsize=9,
                fontweight='bold' if algo == 'MLFQ' else 'normal')

# MLFQ 短作业参考虚线
ax.axhline(y=1.3, color='black', linewidth=1.8, linestyle='--',
           alpha=0.55, zorder=4)
ax.text(2.68, 1.3 + 3.0,
        'MLFQ 短作业基准: 1.3 tick',
        fontsize=8.5, style='italic', color='black', alpha=0.7)

# 组分隔线（灰色竖线）
for x_center in [0.5, 1.5]:
    ax.axvline(x=x_center, color='#CCCCCC', linewidth=0.8,
               linestyle='-', alpha=0.4, zorder=1)

# 坐标轴
ax.set_xticks(group_centers)
ax.set_xticklabels(job_types, fontsize=11)
ax.set_ylabel('响应时间 / tick', fontsize=11)
ax.set_ylim(0, 110)
ax.set_yticks([0, 20, 40, 60, 80, 100])

# 图例
legend = ax.legend(
    loc='upper center',
    bbox_to_anchor=(0.5, -0.13),
    ncol=3,
    frameon=True,
    edgecolor='black',
    fontsize=9,
    handlelength=2.2,
    handleheight=0.8,
)

# 标题
ax.set_title('图 S4-1  MLFQ / RR / FCFS 调度算法响应时间对比',
              fontsize=14, fontweight='bold', pad=18)

# 图注
note_text = (
    '实验环境：QEMU 单核（hart 0），xv6-riscv，8 个并发作业（3 短 + 3 中 + 2 长）\n'
    'MLFQ 五级反馈队列：Q0=1 tick、Q1=2 tick、Q2=4 tick、Q3=8 tick、Q4=15 tick\n'
    '结论：MLFQ 短作业响应最优（1.3 tick），平均响应时间 6.3 tick，综合性能最佳'
)
fig.text(0.5, 0.01, note_text,
         ha='center', va='bottom',
         fontsize=8.5, style='italic',
         multialignment='center',
         fontproperties=prop if font_path else None)

plt.tight_layout(rect=[0, 0.10, 1, 1])

out_dir = '/home/tfc/OS/OS_xv6_riscv/docx/tfc/log'
plt.savefig(f'{out_dir}/fig_s4_1.pdf', dpi=300, bbox_inches='tight')
plt.savefig(f'{out_dir}/fig_s4_1.png', dpi=150, bbox_inches='tight')
print(f"Done: {out_dir}/fig_s4_1.pdf and fig_s4_1.png saved.")
