#define NCPU 4
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

void uart_puts(const char *s);

void start(void) {
    uart_puts("Hello OS\n");
    while (1) {
    }
}