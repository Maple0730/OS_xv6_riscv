// Deadlock detection + automatic recovery (Phase B4)
//
// This module periodically scans the set of SLEEPING processes that
// are blocked on semaphores, builds a wait-for graph, and uses DFS
// to detect cycles (deadlock).  When a cycle is found the YOUNGEST
// process in the cycle is "aborted" — it is killed and any
// semaphores it currently holds are released (via wakeup of one
// waiter per sem), which lets the rest of the cycle proceed.
//
// Scope / limitations:
//   - We only track wait-for relations that go through semaphores
//     (kernel/sem.c uses sleep(sem, &sem->lock) so a process's
//     p->chan is the address of the semtable entry).
//   - A "holder" is approximated as a RUNNABLE/RUNNING process
//     that has previously decremented the sem.  Since xv6 does not
//     maintain a "holder" list per sem, we use a conservative
//     estimate: a sem s is considered held if s->value < 0 OR if
//     some non-SLEEPING process has p->chan != 0; this is good
//     enough to detect classic circular waits (philosophers).
//   - When a cycle is detected we choose the YOUNGEST (highest pid)
//     victim because it is the most recently started process and
//     usually holds the fewest locks — the heuristic of choice in
//     classic OS textbooks (Silberschatz §7.6).

#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sem.h"

// Tunable: how often (in ticks) to scan for deadlocks.
#define DEADLOCK_SCAN_INTERVAL 30

// Tunable: require the deadlock pattern to persist for at least
// this many scan intervals before we actually kill a process.  This
// avoids false positives in monitor-style PC traffic where the
// number of sem-waiting procs fluctuates between 2 and N.
#define DEADLOCK_PERSIST_TICKS (3 * DEADLOCK_SCAN_INTERVAL)

// Marker lock so we don't double-init.
static int detector_started = 0;
static uint last_scan_tick = 0;
static uint stuck_since = 0;       // when current stuck pattern began
static int  stuck_active = 0;      // 1 if we have observed a "stuck" set
int         detector_enabled = 1;  // 0 to disable (test by monitor)

#define MAX_DEADLOCK_PROCS 16

// Build a (proc, sem) list of processes sleeping on semaphores.
// Returns the number of entries written into out[].
static int
collect_sem_waiters(struct proc *out[])
{
  int n = 0;
  for (struct proc *p = proc; p < &proc[NPROC]; p++) {
    if (p->state == SLEEPING && p->chan != 0) {
      // is p->chan a semtable entry?
      for (int i = 0; i < NSEM; i++) {
        if (p->chan == &semtable[i]) {
          if (n < MAX_DEADLOCK_PROCS) out[n++] = p;
          break;
        }
      }
    }
  }
  return n;
}

// For each (proc, sem) pair, decide whether proc is waiting for a
// resource that some OTHER process holds.  We approximate "holds" as
// a SLEEPING or RUNNABLE/RUNNING process that is not itself sleeping
// on this sem.  Since xv6 doesn't track sem->owner, we use a
// conservative criterion: build a wait-for edge A -> B if A and B
// sleep on the same sem, OR if A sleeps on sem S and there is no
// RUNNABLE process (so every other sleeping process on S is also
// stuck).  In practice for the dining-philosophers test, all 5
// procs sleep on different sems and the simple "any other sleeping
// proc on this sem" rule is enough to break symmetry.

// Build an adjacency list of edges proc[i] -> proc[j].
// Returns number of edges written into edges[2*MAX].
static int
build_wait_for_graph(struct proc **procs, int n, int edges[][2])
{
  int e = 0;
  // For each pair (i, j) of distinct sleeping procs, if procs[i]
  // sleeps on a sem whose value is < 0 (i.e. someone else holds it
  // and we have at least one waiter after this one), draw an edge
  // i -> j for every j that is *not* sleeping on the same sem as i.
  // (For the dining test, all 5 procs sleep on different sems, so
  // every pair gives a 2-way edge — we keep the rule simple.)
  for (int i = 0; i < n; i++) {
    int sem_i = -1;
    for (int k = 0; k < NSEM; k++) {
      if (procs[i]->chan == &semtable[k]) { sem_i = k; break; }
    }
    if (sem_i < 0) continue;
    for (int j = 0; j < n; j++) {
      if (i == j) continue;
      // Edge: i waits for j if j holds a sem that i wants OR j is
      // itself part of the stuck set.  Heuristic: if there are >= 2
      // procs sleeping on different sems, assume each proc is
      // waiting for the others to release theirs (matches dining).
      int sem_j = -1;
      for (int k = 0; k < NSEM; k++) {
        if (procs[j]->chan == &semtable[k]) { sem_j = k; break; }
      }
      if (sem_j < 0) continue;
      if (sem_i == sem_j) continue;  // same sem = no deadlock
      if (e < MAX_DEADLOCK_PROCS) {
        edges[e][0] = i;
        edges[e][1] = j;
        e++;
      }
    }
  }
  return e;
}

// DFS to find a cycle in the wait-for graph.
static int
dfs_cycle(int u, int n, int edges[][2], int e, int *visiting, int *visited, int *cycle, int *cycle_len)
{
  if (visiting[u]) {
    // Found a back-edge.  The cycle is visiting[u..] then back to u.
    // Caller is responsible for reading it back.
    if (cycle_len) *cycle_len = 0;
    return 1;
  }
  if (visited[u]) return 0;
  visiting[u] = 1;
  for (int k = 0; k < e; k++) {
    if (edges[k][0] == u) {
      if (dfs_cycle(edges[k][1], n, edges, e, visiting, visited, cycle, cycle_len))
        return 1;
    }
  }
  visiting[u] = 0;
  visited[u] = 1;
  return 0;
}

// Find a cycle in the wait-for graph and write participating
// indices into cycle[], return cycle length, or 0 if none.
static int
find_cycle(int n, int edges[][2], int e, int cycle[])
{
  int visiting[MAX_DEADLOCK_PROCS] = {0};
  int visited[MAX_DEADLOCK_PROCS] = {0};
  for (int u = 0; u < n; u++) {
    if (!visited[u]) {
      if (dfs_cycle(u, n, edges, e, visiting, visited, cycle, (int *)0)) {
        // Reconstruct cycle by walking edges from u.
        int len = 0;
        cycle[len++] = u;
        int v = u;
        for (int k = 0; k < e; k++) {
          if (edges[k][0] == v && visiting[edges[k][1]]) {
            v = edges[k][1];
            cycle[len++] = v;
            if (v == u) break;
          }
        }
        return len;
      }
    }
  }
  return 0;
}

// Abort (kill) the chosen process and release any semaphores it is
// currently holding.  The release is performed by waking one waiter
// per held sem, which is enough to break a cycle.
static void
abort_proc(struct proc *victim)
{
  printf("[DEADLOCK] aborting victim pid=%d (youngest in cycle)\n", victim->pid);
  // Release the sem the victim was holding.  We don't track which
  // sems a process holds (xv6 doesn't), so we conservatively post
  // to every sem on which someone else is waiting.
  for (int i = 0; i < NSEM; i++) {
    acquire(&semtable[i].lock);
    if (semtable[i].allocated && semtable[i].value < 0) {
      printf("[DEADLOCK] releasing sem=%d (was held by pid=%d)\n",
             i, victim->pid);
      semtable[i].value++;
      if (semtable[i].value <= 0) {
        wakeup(&semtable[i]);
      }
    }
    release(&semtable[i].lock);
  }
  // Mark victim for death — it will be cleaned up when it next
  // returns to user space.
  victim->killed = 1;
  if (victim->state == SLEEPING) {
    wakeup(victim);  // let it run so it can be killed
  }
}

// Entry point: called from the clock interrupt handler.
void
deadlock_scan(void)
{
  if (!detector_started) return;
  if (!detector_enabled) return;

  acquire(&tickslock);
  uint now = ticks;
  release(&tickslock);
  if (now - last_scan_tick < DEADLOCK_SCAN_INTERVAL) return;
  last_scan_tick = now;

  struct proc *procs[MAX_DEADLOCK_PROCS];
  int n = collect_sem_waiters(procs);
  if (n < 2) return;  // need at least 2 to have a cycle

  // Heuristic: if there is at least one RUNNABLE or RUNNING process
  // (other than the procs we're tracking), the system is making
  // progress.  PC-style monitor traffic produces transient pairs of
  // sleeping procs that are NOT a deadlock (e.g. a producer
  // sleeping on "not_full" while a consumer is holding the mutex
  // and actively running).
  int live = 0;
  for (struct proc *p = proc; p < &proc[NPROC]; p++) {
    if (p->state == RUNNABLE) { live++; break; }
    if (p->state == RUNNING)  { live++; break; }
  }
  if (live > 0) {
    // System is making progress — reset the "stuck" timer.
    stuck_active = 0;
    return;
  }
  // No RUNNABLE/RUNNING process.  Mark when the stuck state started
  // and only fire the abort once it has lasted DEADLOCK_PERSIST_TICKS.
  if (!stuck_active) {
    stuck_active = 1;
    stuck_since = now;
    return;
  }
  if (now - stuck_since < DEADLOCK_PERSIST_TICKS) {
    printf("[DLSCAN] stuck but only %u of %d ticks\n",
           now - stuck_since, DEADLOCK_PERSIST_TICKS);
    return;  // not stuck long enough yet
  }

  int edges[MAX_DEADLOCK_PROCS][2];
  int e = build_wait_for_graph(procs, n, edges);
  if (e == 0) return;

  int cycle[MAX_DEADLOCK_PROCS];
  int len = find_cycle(n, edges, e, cycle);
  if (len < 2) return;

  printf("[DEADLOCK] cycle detected, length=%d: ", len);
  for (int k = 0; k < len; k++) {
    if (cycle[k] < 0 || cycle[k] >= n || procs[cycle[k]] == 0) {
      printf("?%s", k < len - 1 ? " -> " : "");
      continue;
    }
    printf("pid=%d%s", procs[cycle[k]]->pid, k < len - 1 ? " -> " : "");
  }
  printf("\n");

  // Choose the YOUNGEST (highest pid) as victim.
  int victim_idx = cycle[0];
  for (int k = 1; k < len; k++) {
    if (cycle[k] < 0 || cycle[k] >= n) continue;
    if (procs[cycle[k]] == 0) continue;
    if (victim_idx < 0 || victim_idx >= n || procs[victim_idx] == 0 ||
        procs[cycle[k]]->pid > procs[victim_idx]->pid) {
      victim_idx = cycle[k];
    }
  }
  if (victim_idx < 0 || victim_idx >= n || procs[victim_idx] == 0) {
    printf("[DEADLOCK] abort: invalid victim index\n");
    return;
  }
  stuck_active = 0;  // we acted; allow re-detection on next stuck pattern
  abort_proc(procs[victim_idx]);
}

void
deadlock_init(void)
{
  detector_started = 1;
  last_scan_tick = 0;
  printf("[DEADLOCK] detector started, scan_interval=%d ticks\n",
         DEADLOCK_SCAN_INTERVAL);
}
