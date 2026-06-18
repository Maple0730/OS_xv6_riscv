# Phase D2 — 消息队列测试日志

> 测试程序：`user/mqtest.c`
> 测试目的：验证 `sys_mq_send` / `sys_mq_recv` 的进程间通信
> 关联设计：[`ProcessMgmt_Scheduling_AdvancedExt.md` §2.3 Phase D2](../ProcessMgmt_Scheduling_AdvancedExt.md)

## 实验背景

**消息队列（Message Queue）特点**：
- 内核维护一个固定大小环形队列
- 消息有类型字段
- 按类型接收
- 比 pipe 灵活：消息有边界、非 FIFO（可按类型取）

**API**：
```c
mq_create(key, size)        // -> mqid
mq_send(mqid, type, buf, n)
mq_recv(mqid, type, buf, n) // 阻塞直到有匹配消息
mq_destroy(mqid)
```

## 预期输出

```
[mq] created mqid=0 (size=1024)
[producer] mq_send type=1, msg="hello-1"
[producer] mq_send type=2, msg="hello-2"
[consumer] mq_recv type=1, msg="hello-1"
[consumer] mq_recv type=2, msg="hello-2"
... 100 轮 ...
[mq] sent=100, received=100
[mq] type filtering works
```

## 运行结果（待填）

- [ ] 消息发送接收正确
- [ ] 按类型过滤生效
- [ ] 阻塞接收生效

## 备注

<!-- 实际运行后在此记录 -->
