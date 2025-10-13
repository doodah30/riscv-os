#include "types.h"
#include "memlayout.h"
#include "defs.h"
#include "kmem.h"
//__attribute__ ((aligned (16))) char stack0[4096 * NCPU];
extern void run_vm_tests(void);
extern char end[];       // 从链接ger脚本中获取 end
void main(void) {
    consoleinit();
    kinit((void*)end, (void*)PHYSTOP);
    run_vm_tests();
    /*
    clear();  // 清屏
    printf("helloos %d %x %s %c\n", 123, 0xABCD, "test", 'A');
    printf_color(RED,"helloos %d %x %s %c\n", 123, 0xABCD, "test", 'A');
    clear_line();
    printf("helloos %d %x %s %c\n", 123, 0xABCD, "test", 'A');
    printf("helloos %d %x %s %c\n", 123, 0xABCD, "test", 'A');
    goto_xy(10, 3);
    clear_line();
    printf_color(GREEN,"helloos %d %x %s %c\n", 123, 0xABCD, "test", 'A');
    */
    while(1){};
}