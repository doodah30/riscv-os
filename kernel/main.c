// kernel/main.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "kmem.h"

extern char end[]; // 从链接器脚本获取

// 引用来自 trap.c 的全局测试计数器
extern volatile int timer_test_interrupt_count;

// 函数原型
void test_timer_interrupt(void);

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
    // 调用时钟中断测试函数
    test_timer_interrupt();
    //timer_test_interrupt_count = 0;
    printf("\nAll tests passed!\nSystem halting.\n");
    // 等待中断发生
    while(1){};
}
// 时钟中断功能测试
//
void test_timer_interrupt(void) {
    printf("Testing timer interrupt...\n");
    
    // 记录测试前的时间
    uint64 start_time = r_time();
    // --- 启动测试 ---
    // 将全局测试计数器设置为 1，"激活" clockintr 中的计数逻辑
    timer_test_interrupt_count = 1;
    // 等待，直到 clockintr 将计数器增加到 6 (表示已经发生了 5 次有效中断)
    while (timer_test_interrupt_count < 6) {
      // 可以在这里执行其他任务，模拟一个忙碌的 CPU
      // 我们用一个简单的延时循环来模拟
      
    }
    // --- 停止测试 ---
    // 将测试计数器设置回 0，让 clockintr 停止计数和打印
    timer_test_interrupt_count = 0;

    // --- 可选：关闭时钟中断，让系统彻底安静下来 ---
    //w_sie(r_sie() & ~SIE_STIE);

    uint64 end_time = r_time();

    printf("\nTimer test completed: 5 interrupts occurred in %lu clock cycles.\n", end_time - start_time);
}