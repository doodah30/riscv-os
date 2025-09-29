#include "types.h"
#include "defs.h"
#define NCPU 4
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

void main(void) {
    consoleinit();
    
    clear();  // 清屏
    printf("helloos %d %x %s %c\n", 123, 0xABCD, "test", 'A');
    printf_color(RED,"helloos %d %x %s %c\n", 123, 0xABCD, "test", 'A');
    clear_line();
    printf("helloos %d %x %s %c\n", 123, 0xABCD, "test", 'A');
    printf("helloos %d %x %s %c\n", -2147483648, 0xABCD, "test", 'A');
    goto_xy(10, 3);
    clear_line();
    printf_color(GREEN,"helloos %d %x %s %c\n", 2147483647, 0xABCD, "test", 'A');
    while(1){};
}