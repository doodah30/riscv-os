// console.c - console device (no lock version)
#include "types.h"
#include "defs.h"

void consoleinit(void) {
  uartinit();
}

// 输出单个字符
void consputc(int c) {
  if (c == '\n') {
    uartputc('\r');  // 回车换行兼容
  }
  uartputc(c);
}

// 清屏（ANSI 转义序列）
void clear(void) {
  // \033[2J 清屏，\033[H 光标回到左上角, \033[3J真正的清屏
  uartputs("\033[3J\033[2J\033[H");
}

// 一个小辅助函数，把整数转成字符串
static int uart_printint_to_buf(char *dst, int num) {
  char tmp[16];
  int i = 0, j;
  if (num == 0) {
    dst[0] = '0';
    return 1;
  }
  while (num > 0) {
    tmp[i++] = '0' + (num % 10);
    num /= 10;
  }
  for (j = 0; j < i; j++) {
    dst[j] = tmp[i - j - 1];
  }
  return i;
}

void goto_xy(int x, int y) {
  char buf[32];
  int i = 0;

  buf[i++] = '\033';
  buf[i++] = '[';

  // 输出 y
  i += uart_printint_to_buf(&buf[i], y);
  buf[i++] = ';';

  // 输出 x
  i += uart_printint_to_buf(&buf[i], x);
  buf[i++] = 'H';

  buf[i] = '\0';
  uartputs(buf);
}

void clear_line() {
  uartputs("\033[2K\r");
}
