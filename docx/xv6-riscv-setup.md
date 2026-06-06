# xv6-riscv Git 拉取与环境配置说明

本文档记录了在当前机器上拉取 `mit-pdos/xv6-riscv`、检查构建环境、编译并启动 xv6 的实际步骤。

## 1. 仓库信息

- 仓库地址：`https://github.com/mit-pdos/xv6-riscv.git`
- 本地路径：`/home/hanshuo/office/OS/xv6-riscv`
- 分支：`riscv`
- 已验证提交：`74f84181a3404d1d6a6ff98d342233979066ebb8`

## 2. 当前机器已验证的环境

以下工具已经存在并通过了实际构建验证：

- `qemu-system-riscv64`
- `riscv64-unknown-elf-gcc`
- `make`
- `perl`
- `bc`
- 宿主机 `gcc`

已验证版本：

- `qemu-system-riscv64 --version`
  - `QEMU emulator version 10.1.0`
- `riscv64-unknown-elf-gcc --version`
  - `riscv64-unknown-elf-gcc (14.2.0+19) 14.2.0`

## 3. Git 拉取步骤

在工作目录下执行：

```bash
cd /home/hanshuo/office/OS
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv
git checkout riscv
```

检查当前分支和提交：

```bash
git branch --show-current
git rev-parse HEAD
git remote -v
```

## 4. 环境检查

先确认关键工具在 `PATH` 中：

```bash
which qemu-system-riscv64
which riscv64-unknown-elf-gcc
which make
which perl
which bc
```

如果工具链前缀不是 `riscv64-unknown-elf-`，可以显式指定：

```bash
make TOOLPREFIX=riscv64-unknown-elf-
```

`xv6-riscv` 的 `Makefile` 也会自动尝试识别以下前缀：

- `riscv64-unknown-elf-`
- `riscv64-elf-`
- `riscv64-none-elf-`
- `riscv64-linux-gnu-`
- `riscv64-unknown-linux-gnu-`

## 5. 编译

在仓库根目录执行：

```bash
cd /home/hanshuo/office/OS/xv6-riscv
make
```

说明：

- `make` 会生成内核文件 `kernel/kernel`
- 首次运行 `make qemu` 时还会继续构建用户程序和 `fs.img`

## 6. 启动 xv6

运行：

```bash
make qemu
```

本机实际验证到的启动输出末尾如下：

```text
xv6 kernel is booting
hart 2 starting
hart 1 starting
init: starting sh
$
```

看到 `$` 提示符表示 xv6 已成功启动到 shell。

退出 QEMU：

```text
Ctrl+a
x
```

如果只是想做一次短时验证，也可以：

```bash
timeout 12s make qemu
```

## 7. 调试

启动 GDB 模式：

```bash
make qemu-gdb
```

查看当前自动分配的 GDB 端口：

```bash
make print-gdbport
```

当前机器这次验证得到的端口是：

```text
26000
```

## 8. 清理构建产物

```bash
make clean
```

## 9. 常见问题

### 9.1 找不到 RISC-V 工具链

现象：

```text
Error: Couldn't find a riscv64 version of GCC/binutils.
```

处理方式：

- 确认 `riscv64-unknown-elf-gcc` 在 `PATH` 中
- 或者通过 `make TOOLPREFIX=...` 显式指定工具链前缀

### 9.2 QEMU 版本过低

`Makefile` 要求：

- `QEMU >= 7.2`

检查命令：

```bash
qemu-system-riscv64 --version
```

### 9.3 `bc` 或 `perl` 缺失

这两个工具会分别用于：

- `bc`：比较 QEMU 版本
- `perl`：生成 `user/usys.S`

缺失时先安装后再重新执行 `make` 或 `make qemu`。

## 10. 本次实际完成情况

本次已经在该工作区完成以下操作：

```bash
cd /home/hanshuo/office/OS
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv
make
timeout 12s make qemu
```

结论：

- 仓库已成功拉取
- 当前机器环境已满足 xv6-riscv 的编译和运行要求
- xv6 已成功启动到 shell 提示符
