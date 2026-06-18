// Banker's algorithm — UNSAFE request test (Phase B3-2)
//
// This test drives the banker into a state where the safety
// algorithm will refuse a request that LOOKS locally valid
// (Request <= Need, Request <= Available) but leads to an unsafe
// global state.
//
// Setup: identical to bankertest.c (5 processes, 3 resources, T0).
// We then ask for a request that, once granted, leaves no process
// able to complete.

#include "kernel/types.h"
#include "user/user.h"

static int maxes[5][3] = {
  {7, 5, 3},
  {3, 2, 2},
  {9, 0, 2},
  {2, 2, 2},
  {4, 3, 3},
};

static int initial_alloc[5][3] = {
  {0, 1, 0},
  {2, 0, 0},
  {3, 0, 2},
  {2, 1, 1},
  {0, 0, 2},
};

int
main(int argc, char *argv[])
{
  int avail[3] = {3, 3, 2};
  int req[3];
  int seq[16];
  int nres = 3;

  printf("=== Banker's Algorithm — UNSAFE request test ===\n");
  printf("Phase B3-2:  prove the banker can refuse an unsafe request\n\n");

  if (banker_init(nres, avail) < 0) exit(1);
  for (int i = 0; i < 5; i++) banker_setmax_alloc(i, maxes[i], initial_alloc[i]);
  printf("[banker] init: T0 state installed (setmax_alloc P0..P4)\n");

  // Show the safe sequence.
  if (banker_safe_sequence(seq) == 0) {
    printf("[banker] initial safe sequence: <");
    for (int i = 0; i < 5; i++) printf("P%d%s", seq[i], i<4?", ":"");
    printf(">\n\n");
  }

  // 1) Sane request: P0 request (0,2,0).  This is the textbook
  //    "safe" request and should be granted.
  printf("Test 1: P0 request (0,2,0)  — should be GRANTED\n");
  req[0]=0; req[1]=2; req[2]=0;
  int r = banker_request(0, req);
  printf("  result: %s\n", r==0 ? "GRANTED" : "REFUSED");
  if (r != 0) { printf("FAIL: expected GRANTED\n"); exit(1); }

  // 2) Try to issue a request that the safety algorithm will refuse.
  //    The "classic" unsafe request is one that leaves Available
  //    exactly equal to zero and no Need can be satisfied.
  //    P0 currently holds (0,3,0); Available = (3,1,2).
  //    Ask P0 to take everything: (3,1,2).
  //    After grant: Available = (0,0,0); Need P0 = (7,2,3);
  //    P1 need = (1,2,2); P2 need = (6,0,0); P3 need = (0,1,1);
  //    P4 need = (4,3,1).  None can finish -> UNSAFE.
  printf("\nTest 2: P0 request (3,1,2)  — should be REFUSED (unsafe)\n");
  req[0]=3; req[1]=1; req[2]=2;
  r = banker_request(0, req);
  printf("  result: %s\n", r==0 ? "GRANTED (UNEXPECTED)" : "REFUSED (correct)");
  if (r == 0) { printf("FAIL: banker should have refused\n"); exit(1); }

  // 3) Another unsafe attempt: P4 request (4,3,1) — would take
  //    everything P4 needs. Available before = (3,1,2).  After
  //    grant: Available = (-1, -2, 1)? No, banker will refuse the
  //    first check (Request > Available on at least one type).
  printf("\nTest 3: P4 request (4,3,1)  — should be REFUSED (request > available)\n");
  req[0]=4; req[1]=3; req[2]=1;
  r = banker_request(4, req);
  printf("  result: %s\n", r==0 ? "GRANTED" : "REFUSED (correct, request > available)");
  if (r == 0) { printf("FAIL: banker should have refused\n"); exit(1); }

  printf("\n=== Banker UNSAFE test PASSED ===\n");
  return 0;
}
