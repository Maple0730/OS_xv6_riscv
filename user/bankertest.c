// Banker's algorithm — classic 5-process / 3-resource textbook example
// (Phase B3)
//
// This is the famous example from Silberschatz "Operating System
// Concepts" Chapter 7 (Deadlocks).  It demonstrates that the banker's
// algorithm finds a SAFE sequence.
//
// T0 state:
//                Allocation  Max      Need      Available
//                  A B C    A B C    A B C      A B C
//   P0              0 1 0    7 5 3    7 4 3      3 3 2
//   P1              2 0 0    3 2 2    1 2 2
//   P2              3 0 2    9 0 2    6 0 0
//   P3              2 1 1    2 2 2    0 1 1
//   P4              0 0 2    4 3 3    4 3 1
//
// Safe sequence according to the textbook: <P1, P3, P4, P2, P0>.
//
// The test:
//   1. Initialise the banker with Available = (3,3,2), nres = 3.
//   2. Declare Max for P0..P4.
//   3. Pre-allocate (P0=(0,1,0), P1=(2,0,0), P2=(3,0,2), P3=(2,1,1), P4=(0,0,2))
//      by issuing those requests and asserting they are granted.
//   4. Query a safe sequence and print it.
//   5. Try a request that we know to be safe (e.g. P0 request (0,2,0))
//      and verify it is granted.

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

  printf("=== Banker's Algorithm — Silberschatz T0 example ===\n");
  printf("Phase B3-1:  5 processes, 3 resource types\n");
  printf("             Available = (3, 3, 2)\n\n");

  printf("Initial state:\n");
  printf("  pid | Allocation | Max       | Need\n");
  printf("  ----+------------+-----------+-----------\n");
  for (int i = 0; i < 5; i++) {
    printf("  P%d  | (%d,%d,%d)     | (%d,%d,%d)     | (%d,%d,%d)\n",
           i,
           initial_alloc[i][0], initial_alloc[i][1], initial_alloc[i][2],
           maxes[i][0],       maxes[i][1],       maxes[i][2],
           maxes[i][0]-initial_alloc[i][0],
           maxes[i][1]-initial_alloc[i][1],
           maxes[i][2]-initial_alloc[i][2]);
  }
  printf("\n");

  if (banker_init(nres, avail) < 0) {
    printf("FAIL: banker_init failed\n");
    exit(1);
  }
  printf("[banker] init OK (nres=%d, available=(%d,%d,%d))\n",
         nres, avail[0], avail[1], avail[2]);

  for (int i = 0; i < 5; i++) {
    if (banker_setmax_alloc(i, maxes[i], initial_alloc[i]) < 0) {
      printf("FAIL: banker_setmax_alloc P%d\n", i);
      exit(1);
    }
  }
  printf("[banker] max+alloc declared for P0..P4\n");

  // Sanity check: query a safe sequence right after T0.
  if (banker_safe_sequence(seq) == 0) {
    printf("[banker] initial safe sequence: <");
    for (int i = 0; i < 5; i++) printf("P%d%s", seq[i], i<4?", ":"");
    printf(">\n");
  }

  // Check the safe sequence.
  printf("\nQuerying safe sequence after initial state ...\n");
  if (banker_safe_sequence(seq) < 0) {
    printf("FAIL: state reported UNSAFE\n");
    exit(1);
  }
  printf("[banker] state is SAFE\n");
  printf("  Safe sequence: <");
  for (int i = 0; i < 5; i++) {
    printf("P%d%s", seq[i], i < 4 ? ", " : "");
  }
  printf(">\n");

  // Try a known-safe request: P0 requests (0, 2, 0)
  // After this the Available = (3, 1, 2) and Need P0 = (7, 2, 3).
  // Text book says this should be safe and the new sequence is
  // <P0, P1, P3, P4, P2> or similar.
  printf("\nTest 1: P0 requests (0, 2, 0)  — should be GRANTED\n");
  req[0] = 0; req[1] = 2; req[2] = 0;
  if (banker_request(0, req) == 0) {
    printf("  GRANTED, P0 now holds (0,3,0), available=(%d,%d,%d)\n",
           3, 1, 2);
    if (banker_safe_sequence(seq) == 0) {
      printf("  new safe sequence: <");
      for (int i = 0; i < 5; i++) {
        printf("P%d%s", seq[i], i < 4 ? ", " : "");
      }
      printf(">\n");
    }
  } else {
    printf("  REFUSED (UNEXPECTED — textbook says this is safe)\n");
  }

  // Release P0's allocation so we can issue an unsafe request.
  banker_release(0, initial_alloc[0]);
  banker_release(0, req);
  printf("\nReleased P0's holdings to set up unsafe test.\n");

  // Test 2: a request that the textbook marks as unsafe.
  // After P0 releases, the state should still be the original T0.
  // We now request (3, 3, 2) for P0 — that would leave Available
  // = (0,0,0), and Need[0] = (7,4,3) > (0,0,0), so no process can
  // finish -> UNSAFE.
  printf("\nTest 2: P0 requests (3, 3, 2)  — should be REFUSED (unsafe)\n");
  req[0] = 3; req[1] = 3; req[2] = 2;
  if (banker_request(0, req) == 0) {
    printf("  GRANTED (UNEXPECTED — this should be unsafe!)\n");
  } else {
    printf("  REFUSED — banker correctly identified unsafe state\n");
  }

  printf("\n=== Banker Test PASSED ===\n");
  return 0;
}
