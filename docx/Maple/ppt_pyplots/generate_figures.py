from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import scienceplots  # noqa: F401
from matplotlib import font_manager
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch


ROOT = Path(__file__).resolve().parent
OUT = ROOT / "out"
ZH_FONT_PATH = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"

PRIMARY = "#1E3A8A"
SECONDARY = "#DBEAFE"
ACCENT = "#F59E0B"
GREEN = "#10B981"
GRAY = "#6B7280"
DARK = "#111827"
BG = "#FFFFFF"

ZH_FONT = font_manager.FontProperties(fname=ZH_FONT_PATH)


def setup_style() -> None:
    plt.style.use(["science", "no-latex"])
    plt.rcParams.update(
        {
            "font.family": "sans-serif",
            "font.sans-serif": ["Noto Sans CJK SC", "DejaVu Sans"],
            "axes.facecolor": BG,
            "figure.facecolor": BG,
            "savefig.facecolor": BG,
            "text.color": DARK,
            "axes.edgecolor": BG,
            "axes.unicode_minus": False,
        }
    )


def init_canvas(title: str):
    fig, ax = plt.subplots(figsize=(16, 9), dpi=180)
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis("off")
    ax.text(
        50,
        96,
        title,
        ha="center",
        va="center",
        fontsize=24,
        weight="bold",
        color=PRIMARY,
        fontproperties=ZH_FONT,
    )
    return fig, ax


def add_box(
    ax,
    x: float,
    y: float,
    w: float,
    h: float,
    text: str,
    fc: str = SECONDARY,
    ec: str = PRIMARY,
    lw: float = 2.0,
    fs: int = 13,
    dashed: bool = False,
):
    box = FancyBboxPatch(
        (x, y),
        w,
        h,
        boxstyle="round,pad=0.02,rounding_size=2.2",
        linewidth=lw,
        edgecolor=ec,
        facecolor=fc,
        linestyle="--" if dashed else "-",
    )
    ax.add_patch(box)
    ax.text(
        x + w / 2,
        y + h / 2,
        text,
        ha="center",
        va="center",
        fontsize=fs,
        color=DARK,
        fontproperties=ZH_FONT,
    )
    return box


def add_arrow(
    ax,
    start: tuple[float, float],
    end: tuple[float, float],
    text: str | None = None,
    color: str = PRIMARY,
    dashed: bool = False,
    rad: float = 0.0,
    fs: int = 11,
):
    arrow = FancyArrowPatch(
        start,
        end,
        arrowstyle="-|>",
        mutation_scale=18,
        linewidth=2.0,
        linestyle="--" if dashed else "-",
        color=color,
        connectionstyle=f"arc3,rad={rad}",
    )
    ax.add_patch(arrow)
    if text:
        mx = (start[0] + end[0]) / 2
        my = (start[1] + end[1]) / 2
        ax.text(
            mx,
            my + 2.5,
            text,
            ha="center",
            va="center",
            fontsize=fs,
            color=color,
            fontproperties=ZH_FONT,
        )


def add_group(ax, x: float, y: float, w: float, h: float, title: str):
    box = FancyBboxPatch(
        (x, y),
        w,
        h,
        boxstyle="round,pad=0.03,rounding_size=2.8",
        linewidth=2.2,
        edgecolor=GRAY,
        facecolor="#F8FAFC",
    )
    ax.add_patch(box)
    ax.text(
        x + 2,
        y + h - 3,
        title,
        ha="left",
        va="center",
        fontsize=15,
        weight="bold",
        color=PRIMARY,
        fontproperties=ZH_FONT,
    )


def save(fig, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUT / f"{name}.png", bbox_inches="tight")
    fig.savefig(OUT / f"{name}.svg", bbox_inches="tight")
    plt.close(fig)


def plot_p3_system_boot() -> None:
    fig, ax = init_canvas("系统启动整体框架")

    xs = [4, 17, 30, 43, 57, 71, 84]
    labels = [
        "QEMU virt\n-kernel 加载",
        "内核镜像\n0x80000000",
        "_entry\nkernel/entry.S",
        "start()\nM 态初始化",
        "main()\n内核主初始化",
        "userinit() / init",
        "sh shell",
    ]

    for x, label in zip(xs, labels):
        add_box(ax, x, 56, 11, 16, label)

    for i in range(len(xs) - 1):
        add_arrow(ax, (xs[i] + 11, 64), (xs[i + 1], 64))

    add_box(ax, 30, 28, 12, 12, "早期栈准备\nstack0 / sp", fc="#EFF6FF")
    add_arrow(ax, (35.5, 56), (36, 40), color=ACCENT)

    add_box(ax, 44, 28, 12, 12, "M -> S\nmstatus / mepc / mret", fc="#FEF3C7", ec=ACCENT)
    add_arrow(ax, (49, 56), (50, 40), color=ACCENT)

    add_box(ax, 58, 28, 12, 12, "保存 DTB 指针\na1 -> boot 参数", fc="#ECFDF5", ec=GREEN)
    add_box(ax, 75, 28, 14, 12, "运行时内存探测\n/memory/reg", fc="#ECFDF5", ec=GREEN)
    add_arrow(ax, (64, 40), (75, 40), text="供后续使用", color=GREEN, dashed=True)

    save(fig, "p3_system_boot_flow")


def plot_p6_memory_overview() -> None:
    fig, ax = init_canvas("内存系统整体结构")

    add_group(ax, 14, 66, 72, 18, "堆级分配")
    add_box(ax, 22, 70, 16, 10, "kmalloc(n)", fc="#E0F2FE")
    add_box(ax, 42, 70, 16, 10, "kmfree(p)", fc="#E0F2FE")
    add_box(ax, 62, 70, 18, 10, "slab / 大对象回退", fc="#E0F2FE")
    add_arrow(ax, (38, 75), (42, 75))
    add_arrow(ax, (58, 75), (62, 75))

    add_group(ax, 10, 40, 80, 18, "虚拟内存管理")
    add_box(ax, 15, 44, 16, 10, "页表创建\nkvmmake / uvmcreate")
    add_box(ax, 35, 44, 16, 10, "地址映射\nwalk / mappages")
    add_box(ax, 55, 44, 16, 10, "地址回收\nuvmunmap / uvmfree")
    add_box(ax, 75, 44, 10, 10, "lazy sbrk", fc="#FEF3C7", ec=ACCENT)
    add_arrow(ax, (31, 49), (35, 49))
    add_arrow(ax, (51, 49), (55, 49))
    add_arrow(ax, (71, 49), (75, 49))

    add_group(ax, 14, 14, 72, 18, "物理页框管理")
    add_box(ax, 18, 18, 18, 10, "运行时内存探测\nDTB /memory/reg", fc="#ECFDF5", ec=GREEN)
    add_box(ax, 40, 18, 14, 10, "kalloc()", fc="#ECFDF5", ec=GREEN)
    add_box(ax, 58, 18, 14, 10, "kfree()", fc="#ECFDF5", ec=GREEN)
    add_box(ax, 74, 18, 8, 10, "freelist\n4KB", fc="#ECFDF5", ec=GREEN, fs=11)
    add_arrow(ax, (36, 23), (40, 23), color=GREEN)
    add_arrow(ax, (54, 23), (58, 23), color=GREEN)
    add_arrow(ax, (72, 23), (74, 23), color=GREEN)

    add_arrow(ax, (50, 66), (50, 58), color=PRIMARY)
    add_arrow(ax, (50, 40), (50, 32), color=PRIMARY)
    ax.text(50, 61, "建立映射/分配对象", ha="center", va="center", fontsize=12, color=PRIMARY, fontproperties=ZH_FONT)
    ax.text(50, 35, "底层页框支撑", ha="center", va="center", fontsize=12, color=PRIMARY, fontproperties=ZH_FONT)

    save(fig, "p6_memory_overview")


def plot_p8_memory_extensions() -> None:
    fig, ax = init_canvas("内存系统扩展")

    add_group(ax, 6, 18, 40, 66, "扩展一：运行时物理内存探测")
    add_box(ax, 11, 66, 14, 10, "启动保存\nDTB 指针", fc="#ECFDF5", ec=GREEN)
    add_box(ax, 29, 66, 12, 10, "解析\n/memory/reg", fc="#ECFDF5", ec=GREEN)
    add_box(ax, 11, 48, 14, 10, "得到\nphys_ram_start", fc="#ECFDF5", ec=GREEN)
    add_box(ax, 29, 48, 12, 10, "得到\nphys_ram_end", fc="#ECFDF5", ec=GREEN)
    add_box(ax, 17, 30, 18, 10, "kinit 按运行时范围\n加入空闲页", fc="#ECFDF5", ec=GREEN)
    add_box(ax, 13, 20, 26, 6, "QEMU -m 128M / 256M 自动适配", fc="#F0FDF4", ec=GREEN, fs=11)
    add_arrow(ax, (25, 71), (29, 71), color=GREEN)
    add_arrow(ax, (21, 66), (18, 58), color=GREEN)
    add_arrow(ax, (35, 66), (35, 58), color=GREEN)
    add_arrow(ax, (18, 48), (24, 40), color=GREEN)
    add_arrow(ax, (35, 48), (28, 40), color=GREEN)
    add_arrow(ax, (26, 30), (26, 26), color=GREEN)

    add_group(ax, 54, 18, 40, 66, "扩展二：kmalloc / kmfree")
    add_box(ax, 60, 66, 12, 10, "kmalloc(n)", fc="#E0F2FE")
    add_box(ax, 76, 66, 12, 10, "大小判断", fc="#E0F2FE")
    add_box(ax, 58, 46, 16, 12, "size-class slab\n32B ~ 2048B", fc="#E0F2FE")
    add_box(ax, 78, 46, 12, 12, "大对象回退\n整页分配", fc="#FEF3C7", ec=ACCENT)
    add_box(ax, 67, 28, 14, 10, "kmfree(p)", fc="#E0F2FE")
    add_box(ax, 61, 16, 26, 8, "空 slab 页归还到底层 kfree", fc="#F8FAFC", ec=PRIMARY, fs=11)
    add_box(ax, 58, 4, 32, 8, "真实接入：struct pipe / struct file", fc="#EFF6FF", ec=PRIMARY, fs=11)
    add_arrow(ax, (72, 71), (76, 71))
    add_arrow(ax, (76, 66), (66, 58), text="<= 2048B", color=PRIMARY, fs=10)
    add_arrow(ax, (82, 66), (84, 58), text="> 2048B", color=ACCENT, fs=10)
    add_arrow(ax, (66, 46), (72, 38))
    add_arrow(ax, (84, 46), (76, 38), color=ACCENT)
    add_arrow(ax, (74, 28), (74, 24))
    add_arrow(ax, (74, 16), (74, 12))

    save(fig, "p8_memory_extensions")


def plot_p10_filesystem_layers() -> None:
    fig, ax = init_canvas("文件系统整体结构")

    add_box(ax, 38, 78, 24, 10, "用户命令\nls / cat / echo / open", fc="#F8FAFC")
    add_box(ax, 38, 64, 24, 10, "sysfile.c\n系统调用入口")
    add_box(ax, 38, 50, 24, 10, "file.c\nstruct file / 偏移 / 引用计数")
    add_box(ax, 38, 36, 24, 10, "fs.c\ninode / 目录项 / 路径解析")
    add_box(ax, 38, 22, 24, 10, "log.c\n事务边界 / 崩溃一致性", fc="#FEF3C7", ec=ACCENT)
    add_box(ax, 38, 8, 24, 10, "bio.c + virtio disk\n块缓存 / 磁盘块读写", fc="#ECFDF5", ec=GREEN)

    for y1, y2 in [(78, 74), (64, 60), (50, 46), (36, 32), (22, 18)]:
        add_arrow(ax, (50, y1), (50, y2))

    add_box(ax, 8, 50, 20, 12, "ofile[NOFILE]\n文件描述符表", fc="#EFF6FF")
    add_box(ax, 72, 50, 20, 12, "lseek\nSEEK_SET/CUR/END", fc="#EFF6FF")
    add_box(ax, 72, 34, 20, 12, "namex / namei\n绝对路径 + 相对路径", fc="#EFF6FF")
    add_arrow(ax, (28, 56), (38, 56), dashed=True, color=PRIMARY)
    add_arrow(ax, (62, 56), (72, 56), dashed=True, color=PRIMARY)
    add_arrow(ax, (62, 40), (72, 40), dashed=True, color=PRIMARY)

    save(fig, "p10_filesystem_layers")


def plot_p12_log_and_dualdisk() -> None:
    fig, ax = init_canvas("日志机制与双独立磁盘")

    add_group(ax, 4, 52, 34, 32, "日志事务机制")
    add_box(ax, 8, 64, 8, 8, "begin_op()", fc="#FEF3C7", ec=ACCENT, fs=11)
    add_box(ax, 18, 64, 8, 8, "log_write()", fc="#FEF3C7", ec=ACCENT, fs=11)
    add_box(ax, 28, 64, 6, 8, "commit()", fc="#FEF3C7", ec=ACCENT, fs=11)
    add_box(ax, 18, 54, 12, 8, "install_trans()", fc="#FEF3C7", ec=ACCENT, fs=11)
    add_box(ax, 8, 54, 8, 8, "recover", fc="#FFF7ED", ec=ACCENT, fs=11)
    add_arrow(ax, (16, 68), (18, 68), color=ACCENT)
    add_arrow(ax, (26, 68), (28, 68), color=ACCENT)
    add_arrow(ax, (31, 64), (24, 62), color=ACCENT)
    add_arrow(ax, (16, 58), (18, 58), color=ACCENT, dashed=True)

    add_group(ax, 42, 42, 24, 42, "根盘 ROOTDEV")
    add_box(ax, 47, 68, 14, 8, "/\nroot namespace", fs=12)
    add_box(ax, 46, 54, 8, 8, "desktop", fc="#EFF6FF")
    add_box(ax, 56, 54, 8, 8, "disk1\n挂载点", fc="#E0F2FE")
    add_arrow(ax, (54, 68), (50, 62))
    add_arrow(ax, (54, 68), (60, 62))

    add_group(ax, 72, 42, 24, 42, "第二盘 DISK1DEV")
    add_box(ax, 77, 68, 14, 8, "第二盘根目录", fs=12)
    add_box(ax, 76, 54, 6, 8, "x", fc="#EFF6FF")
    add_box(ax, 86, 54, 6, 8, "...", fc="#EFF6FF")
    add_arrow(ax, (84, 68), (79, 62))
    add_arrow(ax, (84, 68), (89, 62))

    add_arrow(ax, (64, 58), (77, 58), text="跨盘切换", color=PRIMARY)
    add_arrow(ax, (77, 52), (61, 46), text="cd .. 返回根盘", color=GRAY, dashed=True, rad=0.15)

    add_box(ax, 46, 22, 18, 10, "文件不能跨盘\nfs.img / fs1.img 独立管理", fc="#F8FAFC", fs=11)
    add_box(ax, 72, 22, 20, 10, "禁止删除 /disk1\n保护挂载点", fc="#F8FAFC", fs=11)

    save(fig, "p12_log_and_dualdisk")


def main() -> None:
    setup_style()
    plot_p3_system_boot()
    plot_p6_memory_overview()
    plot_p8_memory_extensions()
    plot_p10_filesystem_layers()
    plot_p12_log_and_dualdisk()
    print(f"generated figures in: {OUT}")


if __name__ == "__main__":
    main()
