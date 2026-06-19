# Phase A2-2 — 优先级继承测试日志

> 测试程序：`user/pi_test.c`
> 测试目的：验证优先级继承机制能缩短高优先级进程的等待时间
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.1 Phase A2](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

**经典优先级反转场景**：
- 高优先级 H 等待锁 L
- 锁 L 被**低优先级** L 持有
- 中优先级进程（不持锁）不断抢占 L
- 结果：H 必须等 L 用完锁 + 所有 M 都跑完

**优先级继承解决方案**：
- H 等待 L 持锁时，临时把 L 的优先级提升到 H
- L 立即抢占 M
- L 尽快释放锁，H 恢复运行

## 预期输出

```
[pi] without inheritance:
  H waiting time: 5000 ticks
[pi] with inheritance:
  H waiting time: 100 ticks
  (继承机制将 H 等待时间从 5000 缩短到 100)
```

## 运行结果（待填）

- [ ] 无继承时 H 等待时间明显长
- [ ] 有继承时 H 等待时间显著缩短

## 备注

<!-- 实际运行后在此记录 -->
