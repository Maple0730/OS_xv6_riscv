//
// setsched — query or change the CPU scheduling algorithm from the shell
// Usage:
//   setsched           show current scheduler
//   setsched <n>       switch to scheduler n (0=RR 1=FCFS 2=MLFQ 3=SJF 4=PRIO)
//   setsched list      list all available schedulers
//
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc <= 1) {
    // No arguments: show current scheduler
    int cur = sched_algorithm(-1);
    if (cur < 0) {
      fprintf(2, "setsched: query failed\n");
      return 1;
    }
    printf("current scheduler: %s (%d)\n", sched_algorithm_name(cur), cur);
    return 0;
  }

  if (strcmp(argv[1], "list") == 0) {
    printf("Available schedulers:\n");
    printf("  0 = RR     (Round Robin)\n");
    printf("  1 = FCFS   (First Come First Served)\n");
    printf("  2 = MLFQ   (Multi-Level Feedback Queue)\n");
    printf("  3 = SJF    (Shortest Job First)\n");
    printf("  4 = PRIO   (Static Priority)\n");
    printf("\nUsage: setsched <num>  to switch\n");
    printf("       setsched         to query\n");
    return 0;
  }

  int algo = atoi(argv[1]);
  if (algo < 0 || algo > 5) {
    fprintf(2, "setsched: invalid algorithm %d (valid: 0-4)\n", algo);
    return 1;
  }

  const char *target_name = sched_algorithm_name(algo);
  int old = sched_algorithm(algo);

  if (old < 0) {
    fprintf(2, "setsched: switch to %s failed\n", target_name);
    return 1;
  }

  printf("switched: %s (%d) -> %s (%d)\n",
         sched_algorithm_name(old), old,
         target_name, algo);
  return 0;
}
