#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

#define ANSI_RESET  "\x1b[0m"
#define ANSI_BLUE   "\x1b[34m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_GRAY   "\x1b[90m"
#define NELEM(x)    (sizeof(x) / sizeof((x)[0]))

static char *root_hidden_names[] = {
  "README",
  "cat",
  "echo",
  "forktest",
  "grep",
  "init",
  "kill",
  "ln",
  "ls",
  "mkdir",
  "rm",
  "sh",
  "stressfs",
  "usertests",
  "grind",
  "wc",
  "zombie",
  "logstress",
  "forphan",
  "dorphan",
  "seektest",
  "pipetest",
  "ps",
  "fcfstest",
  "mlfqtest",
  "csw",
  "throughput",
  "halt",
  "lseektest",
  "semtest1",
  "semtest2",
  "semtest3",
  "udptest",
  "waitpidtest",
  "schedtest",
  "shmtest",
  "timeslicetest",
  "cgettime",
  "schedstat",
  "schedlatency",
  "sjftest",
  "sjfmin",
  "sjfbusy",
  "dining",
  "dining_safe1",
  "dining_safe2",
  "bankertest",
  "banker_unsafe",
  "dining_auto",
  "monitortest",
  "pc_monitor",
  "prioritytest",
  "pathfinder",
  "rmtest",
  "edftest",
  "rttest",
  "cpuaffinity",
  "msgqtest",
  "iotest",
  "alltest",
  "setsched",
  "console",
};

static int should_show(char *name, int in_root, int show_all);
static int is_root_hidden(char *name);
static int is_hidden_name(char *name);
static char *name_color(struct stat *st, int hidden);
static void print_entry(char *name, struct stat *st, int hidden);

char *
fmtname(char *path)
{
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return just the filename, no padding.
  if (strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p) + 1);  // copy including null terminator
  return buf;
}

void
ls(char *path, int show_all)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  int is_root_dir;

  if ((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  is_root_dir = (st.type == T_DIR && st.dev == ROOTDEV && st.ino == ROOTINO);

  switch (st.type) {
  case T_DEVICE:
  case T_FILE:
    print_entry(fmtname(path), &st, is_hidden_name(fmtname(path)));
    break;

  case T_DIR:
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if (!should_show(p, is_root_dir, show_all))
        continue;
      if (stat(buf, &st) < 0) {
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      print_entry(fmtname(buf), &st, is_hidden_name(p) ||
                                      (is_root_dir && is_root_hidden(p)));
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i, show_all, paths;

  show_all = 0;
  paths = 0;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-a") == 0) {
      show_all = 1;
      continue;
    }
    paths++;
  }

  if (paths == 0) {
    ls(".", show_all);
    exit(0);
  }

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-a") == 0)
      continue;
    ls(argv[i], show_all);
  }
  exit(0);
}

static int
should_show(char *name, int in_root, int show_all)
{
  if (show_all)
    return 1;
  if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    return 0;
  if (is_hidden_name(name))
    return 0;
  if (in_root && is_root_hidden(name))
    return 0;
  return 1;
}

static int
is_root_hidden(char *name)
{
  for (int i = 0; i < NELEM(root_hidden_names); i++) {
    if (strcmp(name, root_hidden_names[i]) == 0)
      return 1;
  }
  return 0;
}

static int
is_hidden_name(char *name)
{
  return name[0] == '.';
}

static char *
name_color(struct stat *st, int hidden)
{
  if (hidden)
    return ANSI_GRAY;
  if (st->type == T_DIR)
    return ANSI_BLUE;
  if (st->type == T_DEVICE)
    return ANSI_YELLOW;
  return "";
}

static void
print_entry(char *name, struct stat *st, int hidden)
{
  char *color = name_color(st, hidden);
  if (color[0] != 0)
    printf("%s%s%s %d %d %d\n", color, name, ANSI_RESET,
           st->type, st->ino, (int)st->size);
  else
    printf("%s %d %d %d\n", name, st->type, st->ino, (int)st->size);
}
