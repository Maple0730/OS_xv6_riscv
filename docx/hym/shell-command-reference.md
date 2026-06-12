# xv6 Shell 命令使用手册

## 目录
- [Shell 快速入门](#shell-快速入门)
- [文件操作](#文件操作)
- [目录操作](#目录操作)
- [进程管理](#进程管理)
- [文本处理](#文本处理)
- [系统信息](#系统信息)
- [调度器测试](#调度器测试)
- [管道测试](#管道测试)
- [Shell 高级特性](#shell-高级特性)

---

## Shell 快速入门

xv6 Shell 启动后显示 `$ ` 提示符。每行输入一条命令，回车执行。

### 基本操作流程

```sh
$ mkdir mydir                    # 新建目录
$ echo hello > mydir/f.txt       # 新建文件并写入内容
$ cat mydir/f.txt                # 查看文件内容
hello
$ ls mydir                       # 列出目录内容
f.txt
$ echo new text > mydir/f.txt    # 覆盖文件内容
$ echo append >> mydir/f.txt     # 追加内容
$ rm mydir/f.txt                 # 删除文件
```

---

## 文件操作

### cat — 显示文件内容

```sh
$ cat filename           # 显示单个文件
$ cat file1 file2        # 连接多个文件并显示
```

### echo — 输出文本

```sh
$ echo hello             # 输出 hello 到屏幕
$ echo hello > file      # 输出到文件（覆盖）
$ echo world >> file     # 输出到文件（追加）
```

### ls — 列出目录内容

```sh
$ ls                     # 列出当前目录
$ ls dirname             # 列出指定目录
```

### rm — 删除文件

```sh
$ rm filename            # 删除文件
$ rm file1 file2         # 删除多个文件
```

### ln — 创建硬链接

```sh
$ ln oldfile newfile     # 创建硬链接
```

### wc — 统计行数、单词数、字符数

```sh
$ wc filename            # 统计文件
$ wc file1 file2         # 统计多个文件
$ echo hello | wc        # 从管道统计
# 输出格式: 行数 单词数 字符数 文件名
```

### grep — 文本搜索

支持 `^` `.` `*` `$` 正则操作符。

```sh
$ grep pattern filename  # 在文件中搜索
$ grep hello file        # 搜索包含 hello 的行
$ cat file | grep word   # 通过管道搜索
```

---

## 目录操作

### mkdir — 创建目录

```sh
$ mkdir dirname          # 创建单个目录
$ mkdir dir1 dir2        # 创建多个目录
```

### cd — 切换目录（Shell 内置命令）

```sh
$ cd dirname             # 进入目录
$ cd ..                  # 返回上级目录
$ cd /                   # 返回根目录
```

> **注意**: `cd` 是 Shell 内置命令，不是独立程序。必须在父进程中执行才能生效。

---

## 进程管理

### ps — 查看进程列表

```sh
$ ps                     # 显示所有进程信息（PID、状态、名称等）
```

### kill — 终止进程

```sh
$ kill pid               # 终止指定 PID 的进程
$ kill pid1 pid2         # 终止多个进程
```

### halt — 关机

```sh
$ halt                   # 安全关闭系统（通过 SBI 调用退出 QEMU）
```

---

## 文本处理

### printf — 格式化输出

```sh
$ printf "format" args   # 类似 C 语言 printf
$ printf "num=%d\n" 42   # 格式化输出
```

---

## 调度器测试

### fcfstest — 先来先服务调度测试

```sh
$ fcfstest               # 测试 FCFS 调度算法
```

### mlfqtest — 多级反馈队列调度测试

```sh
$ mlfqtest               # 测试 MLFQ 调度算法
```

### csw — 上下文切换统计

```sh
$ csw                    # 上下文切换测试
```

### throughput — 吞吐量测试

```sh
$ throughput             # 系统吞吐量基准测试
```

---

## 管道测试

### pipetest — 管道通信测试

```sh
$ pipetest               # 运行 3 个管道测试
                         #   Test 1: ping-pong (父↔子通信)
                         #   Test 2: 大量数据传输
                         #   Test 3: 多消息收发
```

---

## Shell 高级特性

### 管道 `|`

将左边命令的标准输出连接到右边命令的标准输入：

```sh
$ ls | grep test         # 列出文件名，过滤含 test 的
$ cat file | wc          # 统计文件行数、单词数
$ echo hello world | wc  # 统计 echo 输出的字数
```

管道支持多级串联：

```sh
$ cat file | grep hello | wc
```

### 重定向 `>` `<` `>>`

| 符号 | 作用 |
|------|------|
| `cmd > file` | 输出重定向到文件（覆盖） |
| `cmd >> file` | 输出重定向到文件（追加） |
| `cmd < file` | 从文件读取输入 |

```sh
$ echo hello > output.txt    # 写入文件
$ cat < input.txt             # 从文件读取
$ echo more >> output.txt     # 追加到文件
```

### 后台执行 `&`

```sh
$ longtask &             # 后台运行，Shell 不等待完成
```

### 顺序执行 `;`

```sh
$ mkdir dir; echo ok > dir/file  # 依次执行多条命令
```

### 组合示例

```sh
# 创建目录结构
$ mkdir project
$ mkdir project/src

# 创建并写入文件
$ echo #include \"user.h\" > project/src/main.c

# 查看文件
$ cat project/src/main.c

# 管道 + 重定向
$ ls project | grep src > result.txt
$ cat result.txt

# 统计代码行数
$ wc project/src/main.c

# 查看进程
$ ps

# 关机
$ halt
```

---

## 命名规则注意事项

xv6 文件系统使用短文件名，建议：
- 文件名不超过 **14 个字符**（`DIRSIZ`）
- 路径不超过 **128 个字符**（`MAXPATH`）
- 避免特殊字符，只用字母、数字、`.`、`_`、`-`

---

## 常见错误处理

| 错误提示 | 原因 |
|---------|------|
| `exec xxx failed` | 命令不存在或不是可执行文件 |
| `xxx: failed to create` | 目录创建失败（可能已存在） |
| `cannot cd xxx` | 目录不存在 |
| `syntax` | 命令行语法错误 |
| `too many args` | 参数超过 10 个 |
