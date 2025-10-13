// printf.c
#include "types.h"
#include "defs.h"
#include <stdarg.h>
#include <stdint.h>

static void printint(int xx, int base, int sign) {
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i = 0;
  unsigned int x;

  if (sign && xx < 0) {
    x = -xx;
  } else {
    x = xx;
  }

  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign && xx < 0)
    buf[i++] = '-';

  while (--i >= 0)
    consputc(buf[i]);
}

static void printull(unsigned long long x, int base, int sign) {
  static char digits[] = "0123456789abcdef";
  char buf[32];
  int i = 0;

  if (sign && (long long)x < 0) {
    // interpret as signed if needed
    unsigned long long ux = (unsigned long long)(-(long long)x);
    x = ux;
    // sign handling: push '-' later
    do {
      buf[i++] = digits[x % base];
    } while ((x /= base) != 0);
    buf[i++] = '-';
  } else {
    do {
      buf[i++] = digits[x % base];
    } while ((x /= base) != 0);
  }

  while (--i >= 0)
    consputc(buf[i]);
}

// 核心函数：处理可变参数列表
void vprintf(const char *fmt, va_list ap) {
    char c;
    for (; (c = *fmt) != 0; fmt++) {
        if (c != '%') {
            consputc(c);
            continue;
        }
        // we encountered '%'
        fmt++;
        if ((c = *fmt) == 0) break;

        // handle "ll" prefix for long long
        int is_ll = 0;
        if (c == 'l' && *(fmt+1) == 'l') {
            is_ll = 1;
            fmt += 2;
            c = *fmt;
            if (c == 0) break;
        }

        switch(c) {
        case 'd': {
            if (is_ll) {
                long long v = va_arg(ap, long long);
                // print using printull with sign handling
                if (v < 0) {
                    consputc('-');
                    unsigned long long uv = (unsigned long long)(-v);
                    printull(uv, 10, 0);
                } else {
                    printull((unsigned long long)v, 10, 0);
                }
            } else {
                int d = va_arg(ap, int);
                printint(d, 10, 1);
            }
            break;
        }
        case 'u': {
            if (is_ll) {
                unsigned long long v = va_arg(ap, unsigned long long);
                printull(v, 10, 0);
            } else {
                unsigned int d = va_arg(ap, unsigned int);
                printint(d, 10, 0);
            }
            break;
        }
        case 'x': {
            if (is_ll) {
                unsigned long long v = va_arg(ap, unsigned long long);
                printull(v, 16, 0);
            } else {
                unsigned int d = va_arg(ap, unsigned int);
                printint(d, 16, 0);
            }
            break;
        }
        case 'p': {
            // pointer -> print as 0x<16 hex digits>
            unsigned long long v = (unsigned long long) va_arg(ap, void*);
            // print 0x prefix
            consputc('0'); consputc('x');
            printull(v, 16, 0);
            break;
        }
        case 's': {
            char *s = va_arg(ap, char*);
            if (s == 0) s = "(null)";
            while (*s) consputc(*s++);
            break;
        }
        case 'c': {
            char ch = va_arg(ap, int); // char is promoted to int
            consputc(ch);
            break;
        }
        case '%': {
            consputc('%');
            break;
        }
        default: {
            // unknown format, print literally % and the char
            consputc('%');
            consputc(c);
            break;
        }
        }
    }
}

void printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}

void printf_color(int color, const char *fmt, ...) {
    va_list ap;
    // 设置颜色
    printf("\033[%dm", color);

    // 格式化输出
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    // 恢复颜色
    printf("\033[0m");
}

void panic(char *s)
{
  printf("panic: %s\n", s);
  for(;;)
    ;
}