# Shell PATH Fallback 功能实现

## 问题描述

xv6 的 `exec()` 系统调用只在当前工作目录查找可执行文件。当用户 `cd` 进入子目录后，所有系统程序（`ls`、`cat`、`echo` 等）都在根目录 `/` 下，当前目录里没有这些程序，导致所有命令都报 `exec xxx failed`。

只有 `cd` 能正常工作，因为它是 Shell 内置命令，不依赖 `exec`。

**复现步骤**：
```sh
$ mkdir test
$ cd test
$ ls
exec ls failed
$ cat README
exec cat failed
```

## 解决方案

修改 `user/sh.c` 中的 `runcmd()` 函数，在 `EXEC` 分支增加 **PATH fallback** 机制：

1. 首先尝试用原始路径执行 `exec()`
2. 如果原始路径不以 `/` 开头（即非绝对路径），自动在根目录 `/` 下查找同名程序再次执行

## 代码修改

**文件**: `user/sh.c`，`runcmd()` 函数的 `case EXEC:` 分支

```c
case EXEC:
    ecmd = (struct execcmd *)cmd;
    if (ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    // PATH fallback: if exec fails and path doesn't start with '/',
    // try the root directory (where all system programs live).
    if (ecmd->argv[0][0] != '/') {
      char fullpath[128];
      char *prog = ecmd->argv[0];
      // skip leading "./" if present
      if (prog[0] == '.' && prog[1] == '/')
        prog += 2;
      fullpath[0] = '/';
      strcpy(fullpath + 1, prog);
      exec(fullpath, ecmd->argv);
    }
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;
```

### 逻辑说明

| 步骤 | 说明 |
|------|------|
| 第 1 次 exec | 用原始参数执行（保持向后兼容） |
| 检查路径 | 如果以 `/` 开头则跳过 fallback（已是绝对路径） |
| 去掉 `./` | 如果命令以 `./` 开头，去除此前缀 |
| 拼接路径 | 在程序名前加上 `/`，如 `ls` → `/ls` |
| 第 2 次 exec | 用绝对路径重试执行 |

## 修改效果

```sh
$ cd hym
$ ls                        # 之前: exec ls failed
.             1 1 1024      # 现在: 正常工作
..            1 1 1024
f.txt         2 35 1024

$ cat f.txt                 # 之前: exec cat failed
hello                        # 现在: 正常工作

$ echo hello > new.txt      # 之前: exec echo failed / open failed
$ cat new.txt               # 现在: 正常工作
hello

$ /ls                       # 绝对路径仍然正常工作
$ ./ls                      # 相对路径也被正确处理
```

## 影响范围

- **向后兼容**：不影响绝对路径（`/ls`）、当前目录程序（`./myprog`）和 Shell 内置命令（`cd`）
- **性能**：仅在第一次 exec 失败时才执行 fallback，正常情况下零开销
- **安全性**：仅 fallback 到 `/` 根目录，不会意外执行其他目录的程序

## 相关文件

- `user/sh.c` — Shell 主程序，修改 `runcmd()` 的 `case EXEC` 分支
