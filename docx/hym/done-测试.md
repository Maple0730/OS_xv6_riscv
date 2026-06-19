## done-测试

### 测试环境
- 分支: `merge-0619`
- 日期: 2026-06-19
- 测试方式: Python pexpect 驱动 QEMU，自动注入命令并捕获输出

### 测试程序

| 程序 | 文件 | 说明 |
|------|------|------|
| `iotest` | `user/iotest.c` | 内核层 I/O 操作测试（O_CREATE, O_TRUNC, O_APPEND, O_RDONLY） |
| `alltest` | `user/alltest.c` | 综合测试（I/O内核 + TFC调度切换 + Shell重定向仿真） |

### 测试结果（43项全部通过）

#### Part 1: I/O 内核操作（14项 PASS）
- O_CREATE|O_WRONLY|O_TRUNC 创建写入 ✓
- O_RDONLY 读取验证 ✓
- O_APPEND 追加写入 ✓
- O_TRUNC 覆盖截断 ✓
- 不存在文件 O_RDONLY 返回错误 ✓
- 多次追加正确 ✓

#### Part 2: TFC 调度切换（13项 PASS）
- sched_algorithm(-1) 查询当前算法 ✓
- sched_algorithm(99) 无效输入返回 -1 ✓
- sched_algorithm(-2) 无效输入返回 -1 ✓
- RR(0) 切换成功 ✓
- FCFS(1) 切换成功 ✓
- MLFQ(2) 切换成功 ✓
- SJF(3) 切换成功 ✓
- PRIO(4) 切换成功 ✓
- 每次切换返回值正确 ✓

#### Part 3: Shell 重定向仿真（9项 PASS）
- fork + close(1) + open(.., O_CREATE|O_TRUNC) 模拟 '>' ✓
- fork + close(0) + open(.., O_RDONLY) 模拟 '<' ✓
- fork + close(1) + open(.., O_APPEND) 模拟 '>>' ✓
- fork + close(0) + open(in) + close(1) + open(out) 组合重定向 ✓

#### Shell 直接测试（7项 PASS）
- `echo hello > shtest.txt` → 文件内容正确 ✓
- `cat shtest.txt` → 输出 hello ✓
- `cat < shtest.txt` → 输出 hello ✓
- `echo second >> shtest.txt` → 追加成功 ✓
- `cat shtest.txt` → 输出 hello\nsecond ✓
- `cat < shtest.txt > shtest_out.txt` → 组合重定向 ✓
- `cat shtest_out.txt` → 输出 hello\nsecond ✓

### 测试报告
详见 `docx/xv6_TFC_Maple_测试报告.md`

---

### 手动测试命令

在宿主机终端中执行以下命令即可覆盖全部测试：

#### 1. 编译 & 启动 xv6
```bash
cd /home/houyiming/桌面/OS_xv6_riscv
make clean
make -j$(nproc)
make qemu
```

#### 2. 进入 xv6 Shell 后，依次输入以下命令

**I/O 重定向测试：**
```sh
iotest                          # 内核层 I/O 操作自动测试
echo hello > shtest.txt         # 输出重定向 >
cat shtest.txt                  # 验证文件内容
cat < shtest.txt                # 输入重定向 <
echo second >> shtest.txt       # 追加重定向 >>
cat shtest.txt                  # 验证追加（应输出 hello 和 second）
cat < shtest.txt > shtest_out.txt  # 组合重定向
cat shtest_out.txt              # 验证组合重定向结果
rm shtest.txt                   # 清理
rm shtest_out.txt
```

**TFC 调度切换测试：**
```sh
alltest                         # 综合自动测试（含调度切换）
setsched                        # 查看当前调度算法
setsched list                   # 列出所有可用算法
setsched 1                      # 切换到 FCFS
setsched 0                      # 切换到 RR
setsched 2                      # 切回 MLFQ
schedtest                       # 调度切换完整压力测试
```

**退出 xv6：**
```sh
halt                            # 关闭 QEMU
```
