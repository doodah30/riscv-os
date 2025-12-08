// kernel/spinlock.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

void initlock(struct spinlock *lk, char *name) {
    lk->name = name;
    lk->locked = 0;
    lk->cpu = 0;
}

// 压栈中断状态：关中断，并增加嵌套计数
void push_off(void) {
    int old = intr_get();
    intr_off(); // 关中断
    if(mycpu()->noff == 0)
        mycpu()->intena = old; // 记录最外层的中断状态
    mycpu()->noff += 1;
}

// 弹栈中断状态：减少嵌套计数，如果计数为0且之前是开中断的，则开中断
void pop_off(void) {
    struct cpu *c = mycpu();
    if(intr_get())
        panic("pop_off - interruptible");
    if(c->noff < 1)
        panic("pop_off");
    
    c->noff -= 1;
    if(c->noff == 0 && c->intena)
        intr_on();
}

// 获取锁
void acquire(struct spinlock *lk) {
    push_off(); // 获取锁前必须关中断，防止死锁
    
    if(holding(lk))
        panic("acquire");

    // 使用 RISC-V 原子指令交换
    // __sync_lock_test_and_set 是 GCC 内置函数，对应 amoswap
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0) {
        // 自旋等待
    }

    // 内存屏障，确保临界区代码不会被重排到 acquire 之前
    __sync_synchronize();

    // 记录持有锁的 CPU
    lk->cpu = mycpu();
}

// 释放锁
void release(struct spinlock *lk) {
    if(!holding(lk))
        panic("release");

    lk->cpu = 0;

    // 内存屏障，确保临界区代码不会被重排到 release 之后
    __sync_synchronize();

    // 原子释放
    __sync_lock_release(&lk->locked);

    pop_off(); // 恢复中断状态
}

// 检查当前 CPU 是否持有该锁
int holding(struct spinlock *lk) {
    int r;
    push_off(); // 保证读取 mycpu() 的原子性
    r = (lk->locked && lk->cpu == mycpu());
    pop_off();
    return r;
}