#include <stdint.h>

#define UART0_ADDR 0x10000000UL
volatile uint8_t * const uart0 = (volatile uint8_t *)UART0_ADDR;

/* 单字节写入（非常简化） */
void uart_putc(char c) {
    /* 对于 QEMU virt 的简单实验，直接写数据寄存器通常就能输出 */
    uart0[0] = (uint8_t)c;
}

/* 字符串输出 */
void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}