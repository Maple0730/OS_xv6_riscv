# 韩硕个人 PPT Python 出图

这一目录使用 `matplotlib + SciencePlots` 生成个人 PPT 中的 5 张结构图。

## 生成内容

- 第 3 页：系统启动整体框架
- 第 6 页：内存系统整体结构
- 第 8 页：内存系统扩展
- 第 10 页：文件系统整体结构
- 第 12 页：日志机制与双独立磁盘

## 依赖

- `matplotlib`
- `SciencePlots`

## 生成命令

```bash
MPLCONFIGDIR=/tmp/mplconfig python3 docx/Maple/ppt_pyplots/generate_figures.py
```

## 输出目录

```text
docx/Maple/ppt_pyplots/out/
```

每张图会同时导出：

- `PNG`
- `SVG`

## 风格说明

- 白色背景
- 深蓝主色
- 浅蓝辅助色
- 中文字体：`Noto Sans CJK SC`

