// kernel/proc.h
#ifndef __PROC_H__
#define __PROC_H__

#include "riscv.h"
#include "spinlock.h"

// 进程状态
enum procstate {
    UNUSED,
    USED,
    SLEEPING,
    RUNNABLE,
    RUNNING,
    ZOMBIE
};

// 上下文切换时保存的寄存器
// callee-saved registers (被调用者保存寄存器)
struct context {
    uint64 ra;
    uint64 sp;

    // callee-saved
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

// 保存每个CPU的状态
struct cpu {
  struct proc *proc;          // 当前在这个CPU上运行的进程
  struct context context;     // 调度器的上下文（在此保存以切换到进程）
  int noff;                   // push_off() 的嵌套深度
  int intena;                 // 在 push_off() 之前中断是否是开启的
};

// 进程控制块 (PCB)
struct proc {
    struct spinlock lock;

    // p->lock 必须在修改以下字段时持有:
    enum procstate state;        // 进程状态
    void *chan;                  // 如果非空，表示正在等待该通道(sleep)
    int killed;                  // 如果非零，表示已被杀掉
    int xstate;                  // 退出状态码 (用于 wait)
    int pid;                     // 进程ID

    // 此时不需要持有锁:
    uint64 kstack;               // 内核栈虚拟地址
    uint64 sz;                   // 进程内存大小 (字节)
    pagetable_t pagetable;       // 用户页表
    struct trapframe *trapframe; // 蹦床页的数据 (中断帧)
    struct context context;      // 进程切换上下文
    struct proc *parent;         // 父进程
    char name[16];               // 进程名 (调试用)
};

#endif