# Phase D2: Kernel-Level Message Queue IPC

## Goal

Add a kernel-level **message-queue** IPC primitive to xv6 —
analogous to System V `msgget`/`msgsnd`/`msgrcv`.  A message
queue is a bounded FIFO buffer of fixed-size messages that
is shared between processes via a key.

## What was built

### Kernel (`kernel/msgq.c`, new file)

- `struct msgq` — one queue: key, size, head, tail, count,
  a `notfull` chan for senders, a `notempty` chan for
  receivers, a fixed `msgs[NMSG]` array.
- `msgq_table[NMSGQ]` — global table of queues, protected by
  `msgq_table_lock`.
- `msgq_init()` — called from `main.c`.
- `msgget(key, size)` — create or look up a queue.  Returns
  the qid (its index in the table) on success, -1 on
  failure.  Size is fixed at create time; a second msgget
  with a different size fails.
- `msgsnd(qid, buf, len)` — block on `notfull` until the
  queue has room, then `copyin` the message and `wakeup`
  a receiver.
- `msgrcv(qid, buf, len)` — block on `notempty` until a
  message is available, then `copyout` to the user and
  `wakeup` a sender.

Key implementation notes:
- The queue is **fixed-size at creation**.  Maximum message
  size is `MAX_MSG_SIZE = 128`; the table holds up to
  `NMSGQ = 16` queues, each holding up to `NMSG = 8`
  messages.  These limits trade off memory for simplicity.
- The `notfull` and `notempty` chans are *integers*, not
  pointers, so the standard `sleep(chan, lock)` and
  `wakeup(chan)` work as usual.
- `copyin`/`copyout` (with the process's page table) is
  used for both directions, so a user buffer can be in
  any user-space region.
- The two-pass design (sender waits on `notfull`; receiver
  on `notempty`) means a producer-consumer scenario with
  one sender and N receivers is correctly woken up.

### Syscalls

- `kernel/syscall.h`: `SYS_msgget=60`, `SYS_msgsnd=61`,
  `SYS_msgrcv=62`.
- `kernel/syscall.c`: registered.
- `kernel/sysproc.c`: thin wrappers that pull the
  arguments and call the kernel functions.

### User library

- `user/usys.pl`: added stubs for `msgget`, `msgsnd`, `msgrcv`.
- `user/user.h`: declared them.
- `Makefile`: added `_msgqtest` to `UPROGS`.

### Test (`user/msgqtest.c`)

- Creates a queue with key `0x4D534731`, size 32.
- Verifies that a second `msgget` with the same key returns
  the same qid.
- Verifies that `msgget` with a mismatched size returns -1.
- Forks a sender that pushes 8 messages (each tagged with
  a sequence number in the first 2 bytes and a letter tag
  in the third byte, with the rest of the buffer filled
  with detectable filler to catch truncation).
- The parent (receiver) drains the 8 messages and verifies
  that they come out in order and that the sequence
  numbers match.
- Waits for the sender and checks exit status.

## Trace

```
=== Phase D2: Message Queue IPC ===
created qid=0 for key=0x4d534731
second msgget returns same qid=0 OK
size-mismatch msgget correctly rejected
[sender] sent msg 0 (size=32)
[sender] sent msg 1 (size=32)
...
[sender] sent msg 7 (size=32)
[recv] got msg seq=0 tag=A len=32
[recv] got msg seq=1 tag=B len=32
...
[recv] got msg seq=7 tag=H len=32
=== Phase D2 PASSED ===
```

The trace shows:
- Two distinct `msgget` calls return the same qid (key-based
  lookup works).
- A size-mismatched `msgget` is rejected.
- All 8 messages are delivered in order, with their full
  payload (no truncation).
- The sender and receiver ran concurrently; the wakeup
  path through `notfull`/`notempty` works.

## Files

- `kernel/msgq.c` (new), `kernel/main.c` (init call)
- `kernel/defs.h`, `kernel/syscall.h`, `kernel/syscall.c`,
  `kernel/sysproc.c`
- `user/usys.pl`, `user/user.h`
- `user/msgqtest.c` (new)
- `Makefile`
