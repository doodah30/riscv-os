// kernel/main.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "kmem.h"

extern char end[]; // 从链接器脚本获取

void main(void) {
    //w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
    // 初始化控制台
    consoleinit();
    printf("booting helloos...\n");
    
    // 初始化物理内存分配器
    kinit((void*)end, (void*)PHYSTOP);
    kvminit();
    kvminithart();
    procinit();      // <--- 新增：初始化进程表
    
    plicinit();      // 中断控制器
    plicinithart();
    trapinit();
    trapinithart();  // 陷阱/异常初始化
    
    printf("kernel init done, starting processes...\n");

    // 2. 创建测试进程
    userinit();      // <--- 新增：创建初始的测试进程

    // 3. 启动调度器 (该函数不会返回)
    scheduler();     
}