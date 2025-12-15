// console.c - console device (no lock version)
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@') 

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

struct {
  struct spinlock lock;

#define INPUT_BUF_SIZE 128
  char buf[INPUT_BUF_SIZE];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} cons;

int
consolewrite(int user_src, uint64 src, int n)
{
  char buf[32];
  int i = 0;

  while(i < n){
    int nn = sizeof(buf);
    if(nn > n - i)
      nn = n - i;
    if(either_copyin(buf, user_src, src+i, nn) == -1)
      break;
    uartwrite(buf, nn);
    i += nn;
  }

  return i;
}

//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dist indicates whether dst is a user
// or kernel address.
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons.lock);
  while(n > 0){
    // wait until interrupt handler has put some
    // input into cons.buffer.
    while(cons.r == cons.w){
      if(killed(myproc())){
        release(&cons.lock);
        return -1;
      }
      sleep(&cons.r, &cons.lock);
    }

    c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

    if(c == C('D')){  // end-of-file
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons.r--;
      }
      break;
    }

    // copy the input byte to the user-space buffer.
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if(c == '\n'){
      // a whole line has arrived, return to
      // the user-level read().
      break;
    }
  }
  release(&cons.lock);

  return target - n;
}
