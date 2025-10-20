// kernel/main.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "kmem.h"

extern char end[]; // 从链接器脚本获取

void main(void) {
    // 初始化控制台
    consoleinit();
    printf("booting helloos...\n");
    
    // 初始化物理内存分配器
    kinit((void*)end, (void*)PHYSTOP);
    
    // 初始化中断控制器
    plicinit();
    plicinithart();

    // 初始化S模式的中断向量
    trapinithart();
    
    // 开启 supervisor 模式的中断
    intr_on();

    printf("setup complete; waiting for interrupts.\n");

    // 等待中断发生
    while(1){};
}