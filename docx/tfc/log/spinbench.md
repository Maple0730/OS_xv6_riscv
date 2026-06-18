# Phase E3-2 — 多核自旋锁压测测试日志

> 测试程序：`user/spinbench.c`
> 测试目的：8 线程同时争抢一把自旋锁，测量竞争激烈时的开销
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.2 Phase E](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

**自旋锁特性**：
- 适合短临界区（持锁时间 < 上下文切换时间）
- 多核下可避免阻塞
- 持锁时间长时浪费 CPU（spin = busy wait）

**测试**：8 线程争抢同一把锁，各做 N 次 lock/unlock。

## 预期输出

```
[spinbench] 8 threads, 10000 lock/unlock each
[spinbench] thread 0: total = 50000 ns
[spinbench] thread 1: total = 50200 ns
...
[spinbench] thread 7: total = 51000 ns
[spinbench] avg = 50400 ns/op
[spinbench] throughput = 80000 ops/sec (8 threads * 10000 / total_time)

对比单核下 8 进程用 sem: ~X ns/op (应明显更慢)
```

## 运行结果（待填）

- [ ] 自旋锁在短临界区下吞吐
- [ ] 与信号量版本对比

## 备注

<!-- 实际运行后在此记录 -->
