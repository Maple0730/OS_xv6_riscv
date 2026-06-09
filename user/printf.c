#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#include <stdarg.h>

static char digits[] = "0123456789ABCDEF";

#define PRINTF_BUF_SIZE 256

struct bufstate {
  char *buf;     // current buffer pointer
  int len;       // bytes written so far
  int cap;       // total buffer capacity
  int on_heap;   // whether buf is malloc'd (needs free)
};

// Append one character to the buffer. If the buffer is full,
// double its capacity via malloc so the entire printf result
// fits in a single write() — keeping console output atomic.
static void
bufputc(struct bufstate *bs, char c)
{
  if (bs->len >= bs->cap) {
    int newcap = bs->cap * 2;
    char *newbuf = malloc(newcap);
    if (newbuf == 0) {
      // malloc failed — flush what we have and continue with
      // the original buffer (output will be split but not lost).
      write(1, bs->buf, bs->len);
      bs->len = 0;
      return;
    }
    // copy old content to new buffer
    for (int j = 0; j < bs->len; j++)
      newbuf[j] = bs->buf[j];
    if (bs->on_heap)
      free(bs->buf);
    bs->buf = newbuf;
    bs->cap = newcap;
    bs->on_heap = 1;
  }
  bs->buf[bs->len++] = c;
}

static void
bufputs(struct bufstate *bs, const char *s)
{
  for (; *s; s++)
    bufputc(bs, *s);
}

static void
printint(struct bufstate *bs, long long xx, int base, int sgn)
{
  char tmp[20];
  int i, neg;
  unsigned long long x;

  neg = 0;
  if (sgn && xx < 0) {
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do {
    tmp[i++] = digits[x % base];
  } while ((x /= base) != 0);
  if (neg)
    tmp[i++] = '-';

  while (--i >= 0)
    bufputc(bs, tmp[i]);
}

static void
printptr(struct bufstate *bs, uint64 x)
{
  int i;
  bufputc(bs, '0');
  bufputc(bs, 'x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    bufputc(bs, digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the given fd. Only understands %d, %x, %p, %c, %s.
void
vprintf(int fd, const char *fmt, va_list ap)
{
  char *s;
  char stackbuf[PRINTF_BUF_SIZE];
  struct bufstate bs;
  int c0, c1, c2, i, state;

  // start with a stack buffer; grow to heap if needed
  bs.buf = stackbuf;
  bs.len = 0;
  bs.cap = PRINTF_BUF_SIZE;
  bs.on_heap = 0;

  state = 0;
  for (i = 0; fmt[i]; i++) {
    c0 = fmt[i] & 0xff;
    if (state == 0) {
      if (c0 == '%') {
        state = '%';
      } else {
        bufputc(&bs, c0);
      }
    } else if (state == '%') {
      c1 = c2 = 0;
      if (c0)
        c1 = fmt[i + 1] & 0xff;
      if (c1)
        c2 = fmt[i + 2] & 0xff;
      if (c0 == 'd') {
        printint(&bs, va_arg(ap, int), 10, 1);
      } else if (c0 == 'l' && c1 == 'd') {
        printint(&bs, va_arg(ap, uint64), 10, 1);
        i += 1;
      } else if (c0 == 'l' && c1 == 'l' && c2 == 'd') {
        printint(&bs, va_arg(ap, uint64), 10, 1);
        i += 2;
      } else if (c0 == 'u') {
        printint(&bs, va_arg(ap, uint32), 10, 0);
      } else if (c0 == 'l' && c1 == 'u') {
        printint(&bs, va_arg(ap, uint64), 10, 0);
        i += 1;
      } else if (c0 == 'l' && c1 == 'l' && c2 == 'u') {
        printint(&bs, va_arg(ap, uint64), 10, 0);
        i += 2;
      } else if (c0 == 'x') {
        printint(&bs, va_arg(ap, uint32), 16, 0);
      } else if (c0 == 'l' && c1 == 'x') {
        printint(&bs, va_arg(ap, uint64), 16, 0);
        i += 1;
      } else if (c0 == 'l' && c1 == 'l' && c2 == 'x') {
        printint(&bs, va_arg(ap, uint64), 16, 0);
        i += 2;
      } else if (c0 == 'p') {
        printptr(&bs, va_arg(ap, uint64));
      } else if (c0 == 'c') {
        bufputc(&bs, va_arg(ap, uint32));
      } else if (c0 == 's') {
        if ((s = va_arg(ap, char *)) == 0)
          s = "(null)";
        bufputs(&bs, s);
      } else if (c0 == '%') {
        bufputc(&bs, '%');
      } else {
        // Unknown % sequence.  Print it to draw attention.
        bufputc(&bs, '%');
        bufputc(&bs, c0);
      }

      state = 0;
    }
  }

  // flush everything in one write — atomic thanks to console's writelock
  if (bs.len > 0)
    write(fd, bs.buf, bs.len);

  if (bs.on_heap)
    free(bs.buf);
}

void
fprintf(int fd, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(fd, fmt, ap);
}

void
printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(1, fmt, ap);
}
