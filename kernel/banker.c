// Banker's algorithm — see banker.h for the API.
//
// This file is intentionally simple and self-contained: it does not
// track *real* xv6 processes; the caller provides virtual PIDs.  The
// classic 5-process / 3-resource textbook example is in
// user/bankertest.c.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "vm.h"
#include "banker.h"

static struct spinlock banker_lock;
static struct banker_state B;
static int initialized = 0;

// Clamp pid into the [0, NPROC_B) range; reject negative.
static int
valid_pid(int pid)
{
  if (pid < 0 || pid >= NPROC_B) return 0;
  return 1;
}

// Recompute need[i] = max[i] - allocation[i].
static void
recompute_need(void)
{
  for (int i = 0; i < B.nproc; i++) {
    for (int j = 0; j < B.nres; j++) {
      B.need[i][j] = B.max[i][j] - B.allocation[i][j];
      if (B.need[i][j] < 0) B.need[i][j] = 0;  // defensive
    }
  }
}

// Safety algorithm: is there a sequence of process completions that
// lets every process finish?  If yes, write the sequence to out_seq.
static int
is_safe(int *out_seq)
{
  int work[NRES];
  int finish[NPROC_B];
  for (int j = 0; j < B.nres; j++) work[j] = B.available[j];
  for (int i = 0; i < B.nproc; i++) finish[i] = 0;

  int found = 1;
  int count = 0;
  int order[NPROC_B];

  while (found) {
    found = 0;
    for (int i = 0; i < B.nproc; i++) {
      if (finish[i]) continue;
      // Is need[i] <= work ?
      int ok = 1;
      for (int j = 0; j < B.nres; j++) {
        if (B.need[i][j] > work[j]) { ok = 0; break; }
      }
      if (ok) {
        for (int j = 0; j < B.nres; j++) work[j] += B.allocation[i][j];
        order[count++] = i;
        finish[i] = 1;
        found = 1;
      }
    }
  }

  if (count != B.nproc) return -1;  // unsafe

  if (out_seq) {
    for (int i = 0; i < B.nproc; i++) out_seq[i] = order[i];
  }
  return 0;
}

int
banker_init(int nres, int *avail)
{
  if (nres <= 0 || nres > NRES) return -1;

  acquire(&banker_lock);
  B.nres = nres;
  B.nproc = 0;
  for (int j = 0; j < NRES; j++) {
    B.available[j] = (j < nres) ? avail[j] : 0;
  }
  for (int i = 0; i < NPROC_B; i++) {
    for (int j = 0; j < NRES; j++) {
      B.max[i][j] = 0;
      B.allocation[i][j] = 0;
      B.need[i][j] = 0;
    }
  }
  initialized = 1;
  release(&banker_lock);
  return 0;
}

int
banker_setmax(int pid, int *max)
{
  if (!initialized) return -1;
  if (!valid_pid(pid)) return -1;

  acquire(&banker_lock);
  for (int j = 0; j < B.nres; j++) B.max[pid][j] = max[j];
  if (pid + 1 > B.nproc) B.nproc = pid + 1;
  recompute_need();
  release(&banker_lock);
  return 0;
}

int
banker_setmax_alloc(int pid, int *max, int *alloc)
{
  if (!initialized) return -1;
  if (!valid_pid(pid)) return -1;

  acquire(&banker_lock);
  for (int j = 0; j < B.nres; j++) {
    B.max[pid][j] = max[j];
    B.allocation[pid][j] = alloc[j];
    if (alloc[j] < 0) B.allocation[pid][j] = 0;
    if (max[j] < alloc[j]) { release(&banker_lock); return -1; }  // invalid
  }
  if (pid + 1 > B.nproc) B.nproc = pid + 1;
  recompute_need();  // re-reads B.nproc to update the right range
  release(&banker_lock);
  return 0;
}

int
banker_request(int pid, int *req)
{
  if (!initialized) return -1;
  if (!valid_pid(pid)) return -1;

  acquire(&banker_lock);

  // Step 1: request <= need ?
  for (int j = 0; j < B.nres; j++) {
    if (req[j] > B.need[pid][j]) { release(&banker_lock); return -1; }
  }
  // Step 2: request <= available ?
  for (int j = 0; j < B.nres; j++) {
    if (req[j] > B.available[j]) { release(&banker_lock); return -1; }
  }

  // Step 3: pretend to allocate, run safety check.
  for (int j = 0; j < B.nres; j++) {
    B.available[j] -= req[j];
    B.allocation[pid][j] += req[j];
    B.need[pid][j] -= req[j];
  }
  int seq[NPROC_B];
  int safe = is_safe(seq);

  if (safe < 0) {
    // Roll back: this is the moment where the banker REFUSES.
    for (int j = 0; j < B.nres; j++) {
      B.available[j] += req[j];
      B.allocation[pid][j] -= req[j];
      B.need[pid][j] += req[j];
    }
    release(&banker_lock);
    return -1;
  }

  release(&banker_lock);
  return 0;
}

int
banker_release(int pid, int *rel)
{
  if (!initialized) return -1;
  if (!valid_pid(pid)) return -1;

  acquire(&banker_lock);
  for (int j = 0; j < B.nres; j++) {
    if (rel[j] < 0) { release(&banker_lock); return -1; }
    if (rel[j] > B.allocation[pid][j]) { release(&banker_lock); return -1; }
    B.allocation[pid][j] -= rel[j];
    B.available[j] += rel[j];
    B.need[pid][j] += rel[j];
  }
  release(&banker_lock);
  return 0;
}

int
banker_safe_sequence(int *out_seq)
{
  if (!initialized) return -1;
  acquire(&banker_lock);
  int seq[NPROC_B];
  int r = is_safe(seq);
  if (r == 0 && out_seq) {
    for (int i = 0; i < B.nproc; i++) out_seq[i] = seq[i];
  }
  release(&banker_lock);
  return r;
}

int
banker_get_state(uint64 user_dst)
{
  if (!initialized) return -1;
  struct banker_state copy;
  acquire(&banker_lock);
  copy = B;
  release(&banker_lock);
  struct proc *p = myproc();
  if (copyout(p->pagetable, user_dst, (char *)&copy, sizeof(copy)) < 0)
    return -1;
  return 0;
}

// Initialization of the lock — called from main() or via a constructor.
void
banker_init_lock(void)
{
  initlock(&banker_lock, "banker");
}
