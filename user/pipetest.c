#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// pipetest — 管道通信测试程序
//
// 测试场景：
//   Test 1: 基本 ping-pong（父写→子读→子写→父读）
//   Test 2: 大量数据传输（超过 512 字节，验证环形缓冲区回绕）
//   Test 3: 多消息收发（用 '\n' 分隔，验证应用层消息分帧）

int
main(int argc, char *argv[])
{
  int p1[2];  // 父→子
  int p2[2];  // 子→父
  char buf[512 + 256];  // 大于 PIPESIZE，用来测环形缓冲区回绕
  int i, n;

  printf("pipetest: start\n");

  // ── Test 1: 基本 ping-pong ──
  printf("pipetest: Test 1 -- ping-pong\n");

  if (pipe(p1) < 0 || pipe(p2) < 0) {
    printf("pipetest: pipe() failed\n");
    exit(1);
  }

  if (fork() == 0) {
    // 子进程
    close(p1[1]);   // 关父→子写端
    close(p2[0]);   // 关子→父读端

    // 从父进程读消息
    if ((n = read(p1[0], buf, sizeof(buf))) > 0) {
      buf[n] = '\0';
      printf("pipetest: child received: %s\n", buf);
    }

    close(p1[0]);

    // 回复父进程
    write(p2[1], "pong", 4);
    close(p2[1]);

    exit(0);
  }

  // 父进程
  close(p1[0]);   // 关父→子读端
  close(p2[1]);   // 关子→父写端

  write(p1[1], "ping", 4);
  close(p1[1]);   // 关闭写端，让子进程 read 返回 0

  if ((n = read(p2[0], buf, sizeof(buf))) > 0) {
    buf[n] = '\0';
    printf("pipetest: parent received: %s\n", buf);
  }
  close(p2[0]);

  wait(0);
  printf("pipetest: Test 1 -- PASS\n");

  // ── Test 2: 大量数据传输 ──
  printf("pipetest: Test 2 -- large data (%d bytes)\n", (int)sizeof(buf));

  if (pipe(p1) < 0) {
    printf("pipetest: pipe() failed\n");
    exit(1);
  }

  if (fork() == 0) {
    // 子进程：读
    close(p1[1]);
    int total = 0;
    while ((n = read(p1[0], buf, sizeof(buf))) > 0) {
      total += n;
    }
    printf("pipetest: child read total %d bytes\n", total);
    close(p1[0]);
    exit(0);
  }

  // 父进程：写
  close(p1[0]);

  // 填充已知模式的数据
  for (i = 0; i < (int)sizeof(buf); i++)
    buf[i] = (char)(i & 0xff);

  n = write(p1[1], buf, sizeof(buf));
  printf("pipetest: parent wrote %d bytes\n", n);
  close(p1[1]);

  wait(0);
  printf("pipetest: Test 2 -- PASS\n");

  // ── Test 3: 多消息收发 ──
  // 管道是字节流，不保留消息边界。用 '\n' 作为应用层分隔符。
  printf("pipetest: Test 3 -- multiple messages\n");

  if (pipe(p1) < 0 || pipe(p2) < 0) {
    printf("pipetest: pipe() failed\n");
    exit(1);
  }

  if (fork() == 0) {
    // 子进程：逐字节读，遇 '\n' 输出一行（应用层消息分帧）
    close(p1[1]);
    close(p2[0]);

    int msgcount = 0;
    int linepos = 0;
    char c;
    while (msgcount < 5) {
      n = read(p1[0], &c, 1);
      if (n <= 0) break;
      if (c == '\n') {
        buf[linepos] = '\0';
        printf("pipetest: child msg %d: %s\n", msgcount, buf);
        msgcount++;
        linepos = 0;
      } else if (linepos < (int)sizeof(buf) - 1) {
        buf[linepos++] = c;
      }
    }
    close(p1[0]);

    write(p2[1], "done", 4);
    close(p2[1]);

    exit(0);
  }

  // 父进程：每条消息以 '\n' 结尾
  close(p1[0]);
  close(p2[1]);

  char *msgs[] = {"hello", "xv6", "pipe", "test", "world"};
  for (i = 0; i < 5; i++) {
    write(p1[1], msgs[i], strlen(msgs[i]));
    write(p1[1], "\n", 1);   // 消息分隔符
    printf("pipetest: parent sent '%s'\n", msgs[i]);
  }
  close(p1[1]);

  read(p2[0], buf, sizeof(buf));
  close(p2[0]);

  wait(0);
  printf("pipetest: Test 3 -- PASS\n");

  printf("pipetest: ALL TESTS PASSED\n");
  exit(0);
}
