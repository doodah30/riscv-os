#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "types.h" // 确保能找到 struct cpu

struct spinlock {
    uint locked;       // 是否被锁住 (0/1)
    
    // 用于调试和死锁检测
    char *name;        // 锁的名字
    struct cpu *cpu;   // 持有该锁的 CPU
};

#endif