// kernel/uart.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// UART 寄存器映射
#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

#define RHR 0 // Receive Holding Register (read mode)
#define THR 0 // Transmit Holding Register (write mode)
#define IER 1 // Interrupt Enable Register
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2 // FIFO Control Register
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1)
#define ISR 2 // Interrupt Status Register
#define LCR 3 // Line Control Register
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7)
#define LSR 5 // Line Status Register
#define LSR_RX_READY (1<<0)
#define LSR_TX_IDLE (1<<5)

struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64 uart_tx_w; // write next to uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE]
uint64 uart_tx_r; // read next from uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE]

extern volatile int panicked; // from printf.c

void uartstart();

void uartinit(void) {
  // disable interrupts.
  WriteReg(IER, 0x00);

  // special mode to set baud rate.
  WriteReg(LCR, LCR_BAUD_LATCH);

  // LSB for baud rate of 38.4K.
  WriteReg(0, 0x03);

  // MSB for baud rate of 38.4K.
  WriteReg(1, 0x00);

  // leave set-baud mode,
  // and set word length to 8 bits, no parity.
  WriteReg(LCR, LCR_EIGHT_BITS);

  // reset and enable FIFOs.
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

  // enable transmit and receive interrupts.
  // === 关键修复：开启 TX 中断 ===
  WriteReg(IER, IER_RX_ENABLE | IER_TX_ENABLE);

  initlock(&uart_tx_lock, "uart");
}

// 这是一个支持缓冲和中断的 putc
// 供 printf (内核) 和 consolewrite (用户) 使用
void uartputc(int c) {
  acquire(&uart_tx_lock);

  if(panicked){
    for(;;)
      ;
  }

  while(1){
    if(uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE){
      // buffer is full.
      // wait for some characters to be sent.
      sleep(&uart_tx_r, &uart_tx_lock);
    } else {
      uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = c;
      uart_tx_w += 1;
      uartstart();
      release(&uart_tx_lock);
      return;
    }
  }
}

// 把缓冲区里的数据塞给 UART 硬件
// 必须持有 uart_tx_lock
void uartstart() {
  while(1){
    if(uart_tx_w == uart_tx_r){
      // buffer is empty.
      return;
    }
    
    if((ReadReg(LSR) & LSR_TX_IDLE) == 0){
      // UART TX FIFO full, stop pushing.
      return;
    }
    
    int c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];
    uart_tx_r += 1;
    
    // wakeup waiting uartputc/uartwrite
    wakeup(&uart_tx_r);
    
    WriteReg(THR, c);
  }
}

// 用户态 write 的底层实现
// 把 buf 里的内容一个个塞进 uartputc
// (xv6 的 consolewrite 实际上是调用 uartputc，而不是自己写一套逻辑)
// 如果你的 console.c 调用的是 uartwrite，请保留这个函数名
void uartwrite(char buf[], int n) {
    for(int i = 0; i < n; i++) {
        uartputc(buf[i]);
    }
}

// 读取一个字符
int uartgetc(void) {
  if(ReadReg(LSR) & LSR_RX_READY){
    return ReadReg(RHR);
  } else {
    return -1;
  }
}

// 中断处理函数
void uartintr(void) {
  // read and process incoming characters.
  while(1){
    int c = uartgetc();
    if(c == -1)
      break;
    consoleintr(c);
  }

  // send buffered characters.
  acquire(&uart_tx_lock);
  uartstart();
  release(&uart_tx_lock);
}