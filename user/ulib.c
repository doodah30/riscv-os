#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

// --- 字符串基础函数 ---

uint strlen(const char *s) {
  int n;
  for(n = 0; s[n]; n++)
    ;
  return n;
}

char* strcpy(char *s, const char *t) {
  char *os;
  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int strcmp(const char *p, const char *q) {
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

void* memset(void *dst, int c, uint n) {
  char *cdst = (char *) dst;
  int i;
  for(i = 0; i < n; i++){
    cdst[i] = c;
  }
  return dst;
}

// --- Printf 实现 ---

static void putc(int fd, char c) {
  write(fd, &c, 1);
}

static void printint(int fd, int xx, int base, int sgn) {
  static char digits[] = "0123456789ABCDEF";
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    putc(fd, buf[i]);
}

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

void printf(const char *fmt, ...) {
  char *s;
  int c, i, state;
  va_list ap;

  va_start(ap, fmt); // 初始化参数列表

  state = 0;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        putc(1, c);
      }
    } else if(state == '%'){
      if(c == 'd'){
        printint(1, va_arg(ap, int), 10, 1); // 编译器自动处理参数获取
      } else if(c == 'x' || c == 'p'){
        printint(1, va_arg(ap, uint64), 16, 0); // 这里的 uint64 是你的类型定义
      } else if(c == 's'){
        s = va_arg(ap, char*);
        if(s == 0)
          s = "(null)";
        while(*s)
          putc(1, *s++);
      } else if(c == 'c'){
        putc(1, va_arg(ap, int)); // char 会被提升为 int
      } else if(c == '%'){
        putc(1, c);
      } else {
        // Unknown % sequence.
        putc(1, '%');
        putc(1, c);
      }
      state = 0;
    }
  }
  va_end(ap);
}