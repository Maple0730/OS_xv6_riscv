// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void); // Fork but panics on failure.
void panic(char *);
struct cmd *parsecmd(char *);
void runcmd(struct cmd *) __attribute__((noreturn));

static void
boot_animation(void)
{
    printf(" /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\ \n");
    printf("( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )\n");
    printf(" > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ < \n");
    printf(" /\\_/\\                                                                        /\\_/\\ \n");
    printf("( o.o )                             __________         _______________       ( o.o )\n");
    printf(" > ^ <        _________________________(_)_  /_____    __  __ \\_  ___/        > ^ < \n");
    printf(" /\\_/\\        __  ___/__  __ \\_  ___/_  /_  __/  _ \\   _  / / /____ \\         /\\_/\\ \n");
    printf("( o.o )       _(__  )__  /_/ /  /   _  / / /_ /  __/   / /_/ /____/ /        ( o.o )\n");
    printf(" > ^ <        /____/ _  .___//_/    /_/  \\__/ \\___/    \\____/ /____/          > ^ < \n");
    printf(" /\\_/\\               /_/                                                      /\\_/\\ \n");
    printf("( o.o )                                                                      ( o.o )\n");
    printf(" > ^ <                                                                        > ^ < \n");
    printf(" /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\ \n");
    printf("( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )\n");
    printf(" > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ < \n");
  printf("\n");printf("\n");printf("\n");printf("\n");
}

static void
shutdown_animation(void)
{
  printf(".''''''''''''.''.''....'.....''..........................''.....'''......''''.......'''''''''''''..'\n");
    printf(".........................................'ljCunx}^'.^}f|i'.........................................\n");
    printf("..................................:i_(||{]i'.....`-llI...\")>}|~\"...................................\n");
    printf("................................,rX)+,.......................'>\\)[?]+`........................'.....\n");
    printf("...........................]f)-_~\"`'............................''.'\"]\\|^..........................\n");
    printf(".......................`:;utl........................................!_)j;:\"........................\n");
    printf("....................`(z/!'...........................................'''..+ccx-`....................\n");
    printf("...................\\Z<'......................................................^I{[...................\n");
    printf(".................!(,...........................................................'}\\,'................\n");
    printf("...............^]|<'..............................................................;[!'.............\n");
    printf(".............'|>`.................................................................^_f<'.............\n");
    printf("............imu`.....................................'I`.',,,\",...'`.................}{`............\n");
    printf("...........'x\\^...............^`......,}..^\"..\"....\"+x<^~+\".`];'l-z`':;;'............,uv,...........\n");
    printf("...........~\\'..............\"{1,....!?/;'-/>.{<.'i]I{!{l`..i{i++![\"[)n}c}'.............>]...........\n");
    printf("..........,Y?...........'1`,{!>..`_]']l]_>(tX?^(<.:n]....:z|0+')ui'`I.',)1(............1c]..........\n");
    printf(".........']>'..........,_|,{\"~;!},..~/<!r\\jt|{`.........................iz:{\"..........`~_..........\n");
    printf(".........<\\;..........\"+;1|.\"}[,....``,;.'^!I............................;'>[^..........^_i.........\n");
    printf("........;\\(;.........'_?.?f................................................;<...........,)~.........\n");
    printf("........'<+'.........,_....................................................;<`..........'_['........\n");
    printf("........`[[`.........,_...................................................'i?\"...........~[`........\n");
    printf("........'1f`.........,+....................................................'+>...........-\\\"........\n");
    printf(".........<_..........,+...........''```'.............'\"::;;;IIII;;;;::,`.....l+,.........i]\"........\n");
    printf(".........<_'........`?:.`i+-1|||((){}[[{1((((_><<<>[))_>>>>i!ll!i>>>>>_{(}<~i^.i~........<+.........\n");
    printf(".........I-,.......:\\){{(l\"^''..............'i-^^\"I_`....................';?^?;.>i.......+?.........\n");
    printf(".........^t~......?i,1`......................{`....-:......................I+`\\/i)......^{_.........\n");
    printf("..........+|^....:)\"('......................<!'.....1^.....................I+.])f(|i'...!j;.........\n");
    printf("..........'ti..'[j|^(......................;+^,)_?+^'t.....................l?,\\)\",l!~^.`]?...........\n");
    printf("...........\"U..l[?f\"_I....................`+i>~...\"_vi)'...................!+.')`(-1\\)!)\\xl..........\n");
    printf("............ix'l_)!.`{...................'>}t)'....l{)-[`.................,]..')'+:!{,'<-{<.........\n");
    printf("............^zi?zrl'.I?..................,()(,......!{{1_`...............+?'..+!.:_>l_>',|_.........\n");
    printf("............r+I'li]^..;]!^..............,\\)[;........^)?<+:'.....`\"\"I}\\[`....'}\".,__)\".Il)_.........\n");
    printf("............-]`>1<)l.....:++~>>iiiiiii>??+i'..........';_~?|||\\\\\\)-_!`.\"?+!^I]I'.,-+!..:r?+.........\n");
    printf("............'z`<\"]||l.....?[I;;;;;i>>l;;'.................................^,^....,t]'..\"(]l.........\n");
    printf(".............}]+.?_-^)\\\\?^...............'..............^~{l.....................;(+!..^)(..........\n");
    printf("..............c-.`)j...................`}'.................\"[....................,_^<^.\"|~..........\n");
    printf("..............+]..'|^..................,?..............'`'._l..:>`...............,(({i'-xl..........\n");
    printf("..............:t...\"(.............':I...`1zr-\\\\`.....\\-'`~;'.....i~..............:\\!..i:f...........\n");
    printf("..............,n?l_?X'...........,?`...........`~--?,..............+l............l?....)<...........\n");
    printf("..............'+(:'.>-..........ii.................................'I>..........']I....u............\n");
    printf("...............`]~..`\\..........[..........'l][[}_!_[}[[}}}+'.......,<..........`\\....!\\............\n");
    printf("................-+..'+~.........]'...',I-+,\",::,\"^`\":;:::IiIII+|{^..,!..........,Q)>~f~.............\n");
    printf("................>{,..;{'........^,..`/)<)f\\}[<+-++1[ii{~]-!1/()(I...............\\!.................\n");
    printf(".................`!(f][?.............'I/nn]<~,'}..<;..[._{jj<!?'...............^n...................\n");
    printf("......................:}!...............>+`.`>1t((cc|)<^...\"[;................'n'............'......\n");
    printf(".......................!{,...............`!+l`..........\">+;..................(i....................\n");
    printf("........................lt`.................`,l?[[[[[[~:\"'...................1~'....................\n");
    printf(".........................\"c`...............................................`|['.....................\n");
    printf("..........................'/?.............................................;]{~......................\n");
    printf("...........................,u]:..........................................~;.1(`.....................\n");
    printf("..........................'jrl,^...........................................'{/j:....................\n");
    printf("..........................f~([..............................................+1<n....................\n");
    printf(".........................:Q~]}.........I.......................l;...........+<]c-...................\n");
    printf("........................1>+_x}..........I)^'................`!{\"............~?_|u`..................\n");
    printf("....................':+\\-'.?-|`...........,-]~>I\"'...`,!><?}l..............'<t-_\"z\\1+\"..............\n");
    printf(".............'i)\\|(}\\+?!..`<]{l...............\"I!ii!!;^'..................>r]?^.:1_?+?)\\\\)!'.......\n");
    printf("........'~vxl.'I1]\"....<>'...:?+x'........................................>U{+`..,_...'])i..^tY_'...\n");
    printf("..';+(f[l>_1\\;..........~!'....:[<|-I'..................................I((_!...`[^........I_-!;,-/{\n");
  printf("\n");printf("\n");printf("\n");printf("\n");
    printf(" /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\ \n");
  printf("( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )\n");
  printf(" > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ < \n");
  printf(" /\\_/\\                                                          /\\_/\\ \n");
  printf("( o.o )                                                        ( o.o )\n");
  printf(" > ^ <       _______  ___  ___  _______        ___    ___       > ^ < \n");
  printf(" /\\_/\\      |   _  \"\\|\"  \\/\"  |/\"     \"|      |\"  |  |\"  |      /\\_/\\ \n");
  printf("( o.o )     (. |_)  :)\\   \\  /(: ______)      ||  |  ||  |     ( o.o )\n");
  printf(" > ^ <      |:     \\/  \\\\  \\/  \\/    |        |:  |  |:  |      > ^ < \n");
  printf(" /\\_/\\      (|  _  \\\\  /   /   // ___)_      _|  /  _|  /       /\\_/\\ \n");
  printf("( o.o )     |: |_)  :)/   /   (:      \"|    / |_/ )/ |_/ )     ( o.o )\n");
  printf(" > ^ <      (_______/|___/     \\_______)   (_____/(_____/       > ^ < \n");
  printf(" /\\_/\\                                                          /\\_/\\ \n");
  printf("( o.o )                                                        ( o.o )\n");
  printf(" > ^ <                                                          > ^ < \n");
  printf(" /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\  /\\_/\\ \n");
  printf("( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )( o.o )\n");
  printf(" > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ <  > ^ < \n");
}

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    exit(1);

  switch (cmd->type) {
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd *)cmd;
    if (ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    // PATH fallback: if exec fails and path doesn't start with '/',
    // try the root directory (where all system programs live).
    if (ecmd->argv[0][0] != '/') {
      char fullpath[128];
      char *prog = ecmd->argv[0];
      // skip leading "./" if present
      if (prog[0] == '.' && prog[1] == '/')
        prog += 2;
      fullpath[0] = '/';
      strcpy(fullpath + 1, prog);
      exec(fullpath, ecmd->argv);
    }
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    close(rcmd->fd);
    if (open(rcmd->file, rcmd->mode) < 0) {
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    if (fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    if (pipe(p) < 0)
      panic("pipe");
    if (fork1() == 0) {
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if (fork1() == 0) {
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    if (fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

int
getcmd(char *buf, int nbuf)
{
  write(2, "$ ", 2);
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if (buf[0] == 0) // EOF
    return -1;
  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while ((fd = open("console", O_RDWR)) >= 0) {
    if (fd >= 3) {
      close(fd);
      break;
    }
  }

  boot_animation();
  chdir("/desktop");

  // Read and run input commands.
  while (getcmd(buf, sizeof(buf)) >= 0) {
    char *cmd = buf;
    while (*cmd == ' ' || *cmd == '\t')
      cmd++;
    if (*cmd == '\n') // is a blank command
      continue;
    if (cmd[0] == 'h' && cmd[1] == 'a' && cmd[2] == 'l' && cmd[3] == 't' &&
        (cmd[4] == '\n' || cmd[4] == ' ' || cmd[4] == '\0')) {
      // Shutdown: call halt() directly in the shell, no need to fork.
      shutdown_animation();
      halt();
    } else if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ') {
      // Chdir must be called by the parent, not the child.
      cmd[strlen(cmd) - 1] = 0; // chop \n
      if (chdir(cmd + 3) < 0)
        fprintf(2, "cannot cd %s\n", cmd + 3);
    } else {
      if (fork1() == 0)
        runcmd(parsecmd(cmd));
      wait(0);
    }
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if (pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd *
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd *)cmd;
}

struct cmd *
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd *)cmd;
}

struct cmd *
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd *)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  if (q)
    *q = s;
  ret = *s;
  switch (*s) {
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if (*s == '>') {
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if (eq)
    *eq = s;

  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if (s != es) {
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd *
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while (peek(ps, es, "&")) {
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if (peek(ps, es, ";")) {
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd *
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if (peek(ps, es, "|")) {
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd *
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while (peek(ps, es, "<>")) {
    tok = gettoken(ps, es, 0, 0);
    if (gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch (tok) {
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE | O_TRUNC, 1);
      break;
    case '+': // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE | O_APPEND, 1);
      break;
    }
  }
  return cmd;
}

struct cmd *
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if (!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if (!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd *
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if (peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd *)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while (!peek(ps, es, "|)&;")) {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0)
      break;
    if (tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if (argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd *
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    return 0;

  switch (cmd->type) {
  case EXEC:
    ecmd = (struct execcmd *)cmd;
    for (i = 0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
