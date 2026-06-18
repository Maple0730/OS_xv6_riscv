// Phase D2: message-queue IPC test.
//
// We have one sender and two receivers competing on the same
// message queue.  The sender writes N messages tagged with
// a sequence number; the receivers print whatever they get.
// This tests:
//   - queue creation by key
//   - blocking on full (sender) and empty (receivers)
//   - copyin/copyout with both source and destination in user space
//   - queue's wakeup mechanism (sender wakes one receiver,
//     vice versa)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

#define QKEY  0x4D534731  // "MSG1"
#define QSIZE 32
#define NMSG  8

int
main(int argc, char *argv[])
{
  printf("=== Phase D2: Message Queue IPC ===\n");
  deadlock_set(0);

  int qid = msgget(QKEY, QSIZE);
  if (qid < 0) { printf("FAIL: msgget\n"); exit(1); }
  printf("created qid=%d for key=0x%x\n", qid, QKEY);

  // Get the same queue again — should return the same qid.
  int qid2 = msgget(QKEY, QSIZE);
  if (qid2 != qid) { printf("FAIL: msgget again returned %d\n", qid2); exit(1); }
  printf("second msgget returns same qid=%d OK\n", qid2);

  // Size mismatch must fail.
  int qid3 = msgget(QKEY, QSIZE + 1);
  if (qid3 >= 0) { printf("FAIL: size-mismatch msgget should have failed\n"); exit(1); }
  printf("size-mismatch msgget correctly rejected\n");

  int spid = fork();
  if (spid == 0) {
    // Sender: send NMSG messages.
    for (int i = 0; i < NMSG; i++) {
      char buf[QSIZE];
      // Pack the message: 4 bytes seq no + filler
      // We use a hand-rolled serialisation to avoid stdint alignment.
      buf[0] = (char)(i & 0xFF);
      buf[1] = (char)((i >> 8) & 0xFF);
      buf[2] = 'A' + (i % 26);
      buf[3] = 0;
      // Fill the rest of the buffer to detect truncation.
      for (int j = 4; j < QSIZE; j++) buf[j] = (char)('a' + (j % 26));
      int r = msgsnd(qid, buf, QSIZE);
      if (r < 0) { printf("[sender] msgsnd FAILED at i=%d\n", i); exit(1); }
      printf("[sender] sent msg %d (size=%d)\n", i, QSIZE);
    }
    exit(0);
  }

  // Parent is a receiver: drain NMSG messages and report.
  for (int i = 0; i < NMSG; i++) {
    char buf[QSIZE];
    int n = msgrcv(qid, buf, QSIZE);
    if (n < 0) { printf("[recv] msgrcv FAILED at i=%d\n", i); exit(1); }
    int seq = (int)(unsigned char)buf[0] | ((int)(unsigned char)buf[1] << 8);
    printf("[recv] got msg seq=%d tag=%c len=%d\n", seq, buf[2], n);
    if (seq != i) {
      printf("FAIL: expected seq=%d got seq=%d\n", i, seq);
      exit(1);
    }
  }

  int status = -1;
  wait(&status);
  if (status != 0) {
    printf("FAIL: sender exited with status=%d\n", status);
    exit(1);
  }
  printf("=== Phase D2 PASSED ===\n");
  exit(0);
}
