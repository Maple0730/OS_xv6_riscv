# Phase F1 — Rate-Monotonic 实时调度测试日志

> 测试程序：`user/rmtest.c`
> 测试目的：验证 RM 调度（周期越短优先级越高）的可调度性
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.2 Phase F1](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

**Rate-Monotonic Scheduling (Liu & Layland, 1973)**：
- 静态优先级 = 周期倒数
- 周期越短，优先级越高
- 可调度条件：∑(Cᵢ/Tᵢ) ≤ n(2^(1/n) - 1)
  - n=1: 1.000
  - n=2: 0.828
  - n=3: 0.779
  - n=∞: 0.693

**测试 3 个周期性任务**：
- T1: period=10, cost=2, util=0.20
- T2: period=20, cost=4, util=0.20
- T3: period=40, cost=8, util=0.20
- 总利用率=0.60 < 0.779 (n=3) -> 可调度

## 预期输出

```
[rm] task 1 (period=10) registered, priority=HIGH
[rm] task 2 (period=20) registered, priority=MID
[rm] task 3 (period=40) registered, priority=LOW
[rm] total utilization = 0.60 (< 0.779) -> schedulable
... 运行 100 个周期 ...
[rm] deadline miss count = 0
[rm] deadline meet rate = 100%
```

## 运行结果（待填）

- [ ] 3 个周期任务按 RM 优先级调度
- [ ] 截止时间达成率 = 100%
- [ ] 验证可调度性公式

## 备注

<!-- 实际运行后在此记录 -->
