#include <stdint.h>
#include "memlayout.h"

#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))

#define RHR 0                 // receive holding register (for input bytes)
#define THR 0                 // transmit holding register (for output bytes)
#define IER 1                 // interrupt enable register
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // FIFO control register
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
#define ISR 2                 // interrupt status register
#define LCR 3                 // line control register
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
#define LSR 5                 // line status register
#define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define LSR_TX_IDLE (1<<5)    // THR can accept another character to send

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

void
uartinit(void)
{
    // 关闭中断
    WriteReg(IER, 0x00);

    // 设置波特率
    WriteReg(LCR, LCR_BAUD_LATCH);
    WriteReg(0, 0x03);
    WriteReg(1, 0x00);

    // 设置线路控制，并关闭波特率设置模式
    WriteReg(LCR, LCR_EIGHT_BITS);

    // 开启并清空 FIFO 缓冲区
    WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

    // 重新开启接收中断 (为之后接收键盘输入做准备)
    WriteReg(IER, IER_RX_ENABLE);
}
/* 单字节写入（非常简化） */
void uartputc(char c) {
    
    while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
    /* 对于 QEMU virt 的简单实验，直接写数据寄存器通常就能输出 */
    WriteReg(THR,c);
}

/* 字符串输出 */
void uartputs(const char *s) {
    while (*s) uartputc(*s++);
}

void uartintr(void)
{
  // 这是一个占位函数，目前什么都不做
  // 之后我们会在这里处理键盘输入
}