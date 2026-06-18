# Phase B3-2 — 银行家算法（不安全请求被拒绝）测试日志

> 测试程序：`user/banker_unsafe.c`
> 测试目的：构造一个让银行家算法拒绝的不安全请求
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.1 Phase B3](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

**构造场景**：使 Available 不足以满足任何进程的全部 Need
- `is_safe_state()` 返回 false
- 银行家算法拒绝该请求

## 预期输出

```
[banker] request: P0 request [3, 3, 2] -> REJECTED
[banker] reason: would lead to UNSAFE state
[banker] safety check: no safe sequence exists with this allocation
[banker] Available = [0, 0, 0]
[banker] P0 Need = [7, 4, 3] > Available
```

## 运行结果

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

- [x] 不安全请求被拒绝
- [x] 输出明确说明拒绝原因（`REFUSED (correct)` / `REFUSED (correct, request > available)`）

## 备注

### 3 种拒绝场景

1. **Step 1 拒绝 — Request > Need**：例如某进程已分配接近 max，再申请更多会被立即拒
2. **Step 2 拒绝 — Request > Available**：资源数不够，本次申请无法满足
3. **Step 3 拒绝 — Safety check**：本次申请虽然 ≤ Need 且 ≤ Available，但模拟后全局进入 unsafe 状态

bankertest / banker_unsafe 都覆盖了这 3 种情况。

### 与 B3 主测试的关系

`banker_unsafe.c` 故意构造"看起来合法但实际 unsafe"的请求（Test 2），证明银行家算法不仅能**找到安全序列**（B3-1），还能**主动拒绝**会破坏安全性的请求（B3-2）。两者结合构成完整的"避免死锁"教学闭环。
