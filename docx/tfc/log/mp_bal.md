# Phase E3-1 — 多核负载均衡测试日志

> 测试程序：`user/mp_bal.c`
> 测试目的：验证 Per-CPU 队列 + Pull 策略的负载均衡效果
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.2 Phase E](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

**场景**：8 worker 进程 + 8 sleep 进程
- 8 worker 持续 spin（CPU 密集）
- 8 sleep 周期唤醒
- 观察：每 CPU 的 runq 长度

**预期（无均衡）**：worker 全部跑在 CPU 0，runq 不均
**预期（有均衡）**：worker 分散到 8 个 CPU，runq 均衡

## 预期输出

```
[mp_bal] start: 8 workers + 8 sleepers
... 每 N tick 输出每 CPU 的 runq 长度 ...
[mp_bal] tick 1000: CPU0=2, CPU1=1, CPU2=1, CPU3=2, CPU4=1, CPU5=1, CPU6=0, CPU7=0
[mp_bal] tick 1100: CPU0=1, CPU1=1, CPU2=1, CPU3=1, CPU4=1, CPU5=1, CPU6=1, CPU7=1
[mp_bal] migration events: 4 (load balancing triggered)
```

## 运行结果（待填）

- [ ] 8 worker 分散到多核
- [ ] runq 长度相对均衡
- [ ] 触发过 pull 迁移

## 备注

<!-- 实际运行后在此记录 -->
