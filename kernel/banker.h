// Banker's Algorithm (Phase B3)
//
// Implements Dijkstra's Banker's Algorithm for deadlock avoidance.
// All resources are unit-counted integers; resource types are fixed
// at compile time (NRES).  Processes are abstracted by *virtual* PIDs
// supplied by the caller (so the test driver can play the role of any
// of the 5 textbook processes without needing real xv6 processes).
//
// System calls (added in syscall.h):
//   banker_init(nres, avail)             -> 0 ok, -1 error
//   banker_setmax(pid, max[])            -> 0 ok, -1 error
//   banker_request(pid, req[])           -> 0 granted, -1 unsafe/refused
//   banker_release(pid, rel[])           -> 0 ok, -1 error
//   banker_safe_sequence(out_seq)        -> 0 safe (sequence written), -1 unsafe
//   banker_get_state(out_state)          -> 0 ok, copies banker_state to user

#ifndef XV6_KERNEL_BANKER_H
#define XV6_KERNEL_BANKER_H

#include "types.h"

#define NRES    8
#define NPROC_B 16   // max virtual PIDs tracked by the banker

struct banker_state {
  int available[NRES];
  int max[NPROC_B][NRES];
  int allocation[NPROC_B][NRES];
  int need[NPROC_B][NRES];
  int nres;
  int nproc;
};

// Initialize the banker.  Must be called before any other banker_* call.
// Returns 0 on success, -1 on error.
int banker_init(int nres, int *avail);

// Declare the maximum demand of a (virtual) process pid.
// (Allocation is expected to start at 0; the caller should issue a
//  request() of size 0 (or use banker_setmax_alloc) to register an
//  initial non-zero allocation.)
// Returns 0 on success, -1 on error.
int banker_setmax(int pid, int *max);

// Same as banker_setmax but also records the *current* allocation.
// This is the right call when initialising a textbook example where
// processes already hold resources at T0.
int banker_setmax_alloc(int pid, int *max, int *alloc);

// Try to grant a request.  Performs the safety check.
// Returns 0 if granted, -1 if the request would leave the system
// in an unsafe state (request refused).
int banker_request(int pid, int *req);

// Release resources.  Returns 0 on success, -1 on error.
int banker_release(int pid, int *rel);

// Check whether the current state is safe.  If so, writes a safe
// sequence (length = banker.nproc) into out_seq (caller must
// provide at least NPROC_B ints).  Returns 0 if safe, -1 if not.
int banker_safe_sequence(int *out_seq);

// Copy the full banker state to user space via copyout.
int banker_get_state(uint64 user_dst);

#endif // XV6_KERNEL_BANKER_H
