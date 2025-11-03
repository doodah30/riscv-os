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
void test_all_exceptions();
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
    //test_timer_interrupt();
    test_all_exceptions();
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

    printf("\nTimer test completed: 5 interrupts occurred in %llu clock cycles.\n", end_time - start_time);
}
// 1. 测试非法指令异常
// 我们定义一个非法的、CPU不认识的指令编码
#define ILLEGAL_INSTRUCTION 0x00000000
void trigger_illegal_instruction() {
    printf("Testing Illegal Instruction Exception...\n");
    printf("This should trigger a panic.\n");
    // 使用内联汇编来插入一个绝对非法的指令
    asm volatile(".word %0" : : "i"(ILLEGAL_INSTRUCTION));
    // 如果内核能从 panic 中恢复，才会执行到这里
    printf("FAIL: Survived an illegal instruction!\n");
}

// 2. 测试内存访问异常 (写)
void trigger_store_fault() {
    printf("\nTesting Store Access Fault...\n");
    printf("Attempting to write to a read-only memory location (address 0x80000000, the kernel text segment)...\n");
    printf("This should trigger a panic.\n");
    // 内核的代码段 (.text) 通常是只读的
    int *p = (int *)0x80000000;
    *p = 42;
    printf("FAIL: Survived writing to read-only memory!\n");
}

// 3. 测试内存访问异常 (读)
void trigger_load_fault() {
    printf("\nTesting Load Access Fault...\n");
    printf("Attempting to read from an invalid memory location (e.g., address 0x0)...\n");
    printf("This should trigger a panic.\n");
    // 地址 0 通常没有被映射
    int p = *(int *)0x0;
    // 使用 p 以防止编译器优化掉这个读取操作
    printf("Value at address 0 is %d (FAIL: Survived reading from invalid memory!)\n", p);
}


// 4. 测试系统调用
void trigger_syscall() {
    printf("\nTesting System Call (ecall)...\n");
    printf("This should be handled gracefully.\n");
    // 使用 ecall 指令触发一个系统调用异常
    asm volatile("ecall");
    printf("SUCCESS: Returned from system call handler!\n");
}


//
// --- 主测试函数 ---
//
void test_all_exceptions() {
    // 首先测试可以正常返回的系统调用
    //trigger_syscall();

    //trigger_store_fault();//存储页故障

    trigger_load_fault();//加载页故障

    //trigger_illegal_instruction();

    printf("\nAll exception tests passed (or panicked as expected)!\n");
}