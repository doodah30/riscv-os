// printf.c
#include "types.h"
#include "defs.h"
#include <stdarg.h>

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

// 核心函数：处理可变参数列表
void vprintf(const char *fmt, va_list ap) {
    char c;
    for(; (c = *fmt) != 0; fmt++){
        if(c != '%'){
            consputc(c);
            continue;
        }
        fmt++;
        if((c = *fmt) == 0) break;

        switch(c){
        case 'd': {
            int d = va_arg(ap, int);
            printint(d, 10, 1);
            break;
        }
        case 'x': {
            int d = va_arg(ap, int);
            printint(d, 16, 0);
            break;
        }
        case 's': {
            char *s = va_arg(ap, char*);
            if(s == 0) s = "(null)";
            while(*s) consputc(*s++);
            break;
        }
        case 'c': {
            char ch = va_arg(ap, int); // char 会提升为 int
            consputc(ch);
            break;
        }
        case '%': {
            consputc('%');
            break;
        }
        default: {
            // 未知格式，直接打印出来
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