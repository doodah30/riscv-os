#include <stdint.h>

#define UART0 0x10000000UL

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

/* 单字节写入（非常简化） */
void uart_putc(char c) {
    
    while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
    /* 对于 QEMU virt 的简单实验，直接写数据寄存器通常就能输出 */
    WriteReg(THR,c);
}

/* 字符串输出 */
void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}