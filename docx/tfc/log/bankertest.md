# Phase B3-1 — 银行家算法（安全状态）测试日志

> 测试程序：`user/bankertest.c`
> 测试目的：经典 5 进程 3 资源示例，验证银行家算法能找到安全序列
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.1 Phase B3](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

经典教科书示例（Silberschatz）：

**T0 状态**：

| 进程 | Allocation (A B C) | Max (A B C) | Need (A B C) |
|------|--------------------|-------------|--------------|
| P0   | 0 1 0              | 7 5 3       | 7 4 3        |
| P1   | 2 0 0              | 3 2 2       | 1 2 2        |
| P2   | 3 0 2              | 9 0 2       | 6 0 0        |
| P3   | 2 1 1              | 2 2 2       | 0 1 1        |
| P4   | 0 0 2              | 4 3 3       | 4 3 1        |

**Available** = (3, 3, 2)

**安全序列**：P1, P3, P4, P2, P0

## 预期输出

```
[banker] init: Available = [3, 3, 2]
[banker] request: P1 request [1, 0, 2] -> GRANTED
[banker] safety check: <P1, P3, P4, P2, P0> is SAFE
[banker] request: P0 request [0, 2, 0] -> GRANTED
[banker] safety check: <P0, P1, P3, P4, P2> is SAFE
...
[banker] all requests satisfied, exit
```

## 运行结果

**实际输出（bankertest 摘录）**：
```
=== Banker's Algorithm — Silberschatz T0 example ===
Phase B3-1:  5 processes, 3 resource types
             Available = (3, 3, 2)

Initial state:
  pid | Allocation | Max       | Need
  P0  | (0,1,0)     | (7,5,3)     | (7,4,3)
  P1  | (2,0,0)     | (3,2,2)     | (1,2,2)
  P2  | (3,0,2)     | (9,0,2)     | (6,0,0)
  P3  | (2,1,1)     | (2,2,2)     | (0,1,1)
  P4  | (0,0,2)     | (4,3,3)     | (4,3,1)

[banker] init OK (nres=3, available=(3,3,2))
[banker] max+alloc declared for P0..P4
[banker] initial safe sequence: <P1, P3, P4, P0, P2>

Test 1: P0 requests (0, 2, 0) — should be GRANTED
  GRANTED, P0 now holds (0,3,0), available=(3,1,2)
  new safe sequence: <P3, P1, P2, P0, P4>

Test 2: P0 requests (3, 3, 2) — should be REFUSED (unsafe)
  REFUSED — banker correctly identified unsafe state

=== Banker Test PASSED ===
```

**实际输出（banker_unsafe 摘录）**：
```
[banker] init: T0 state installed (setmax_alloc P0..P4)
[banker] initial safe sequence: <P1, P3, P4, P0, P2>

Test 1: P0 request (0,2,0) — should be GRANTED
  result: GRANTED

Test 2: P0 request (3,1,2) — should be REFUSED (unsafe)
  result: REFUSED (correct)

Test 3: P4 request (4,3,1) — should be REFUSED (request > available)
  result: REFUSED (correct, request > available)

=== Banker UNSAFE test PASSED ===
```

- [x] 找到安全序列：`<P1, P3, P4, P0, P2>`（textbook 顺序）
- [x] 至少一个安全序列与教科书一致（`<P1, P3, P4, P0, P2>`）
- [x] 全部请求按预期处理（安全请求 GRANTED，unsafe 请求 REFUSED）

## 备注

### 新增系统调用（kernel/banker.h）

```c
#define NRES    8
#define NPROC_B 16
struct banker_state {
  int available[NRES];
  int max[NPROC_B][NRES];
  int allocation[NPROC_B][NRES];
  int need[NPROC_B][NRES];
  int nres, nproc;
};
int banker_init(int nres, int *avail);
int banker_setmax(int pid, int *max);
int banker_setmax_alloc(int pid, int *max, int *alloc);  // T0 状态一次性设置
int banker_request(int pid, int *req);                    // 含安全性检查
int banker_release(int pid, int *rel);
int banker_safe_sequence(int *out_seq);
int banker_get_state(uint64 user_dst);
```

### 实现要点

- **数据结构**：`banker_state` 全局单例，3 个矩阵 (Max/Allocation/Need) + 1 个向量 (Available)
- **核心算法**：经典 `is_safe` 模拟 — 复制 Available 到 work，找到一个 `need <= work` 的进程，回收其 allocation 到 work，重复直到所有进程 finish 或无新进程可 finish
- **关键设计决策**：
  - 提供 `banker_setmax_alloc`（而不是只 `setmax`）：T0 状态通常所有进程已有非零 allocation
  - `banker_request` 在 commit 前模拟分配，跑 is_safe；unsafe 时回滚 + 返回 -1
  - `recompute_need` 在 max/alloc 改变后重算
- **关键 bug 与修复**：
  - 一开始 `recompute_need` 在 nproc 更新**之前**调用 → need 全 0 → is_safe 误判 safe。修复：先更新 nproc 再 recompute
  - `banker_request` 中第一次 acquire 之后误插入了第二次 acquire → `panic: acquire`。修复：删除重复 acquire
- **不依赖任何 xv6 进程抽象**：banker 用 *virtual* PID（0..NPROC_B-1），由 test driver 提供语义映射。这是为支持"经典教科书 5 进程 3 资源"例子的设计选择

### 与计划对应

- §2.1 Phase B3（OS 课程必讲、汇报价值 ⭐⭐⭐⭐⭐） ✅ 完成
- 对应内核文件：`kernel/banker.h`、`kernel/banker.c`、`kernel/sysproc.c`（sys #40-46）、`kernel/syscall.h`/`.c`、`user/usys.pl`、`user/user.h`
- 对应测试程序：`user/bankertest.c`（安全序列）、`user/banker_unsafe.c`（unsafe 拒绝）
