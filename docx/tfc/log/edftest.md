# Phase F2 — Earliest Deadline First (EDF) 实时调度测试日志

> 测试程序：`user/edftest.c`
> 测试目的：验证 EDF 调度（动态优先级 = 截止时间最近）
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.2 Phase F2](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

**Earliest Deadline First (EDF)**：
- 动态优先级：截止时间最近的任务优先级最高
- 可调度条件：∑(Cᵢ/Tᵢ) ≤ 1
- 实现：用截止时间最小堆

**对比 RM**：
- RM 优先级静态（按周期），不能利用全部利用率
- EDF 优先级动态，能达到 100% 利用率

## 预期输出

```
[edf] task 1 (period=10, cost=2) registered
[edf] task 2 (period=20, cost=4) registered
[edf] task 3 (period=40, cost=8) registered
[edf] total utilization = 0.60 (< 1.0) -> schedulable
... 运行 ...
[edf] always pick task with earliest deadline
[edf] deadline miss count = 0
```

## 运行结果（待填）

- [ ] EDF 按截止时间排序选择
- [ ] 截止时间达成率 = 100%
- [ ] 与 RM 行为对比（EDF 更灵活）

## 备注

<!-- 实际运行后在此记录 -->
