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

// 陷阱帧：用于保存用户态寄存器
// 这个结构体必须与 trampoline.S 中的汇编偏移量严格一致
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // 内核页表 (由 usertrapret 设置)
  /*   8 */ uint64 kernel_sp;     // 进程的内核栈顶 (由 usertrapret 设置)
  /*  16 */ uint64 kernel_trap;   // usertrap() 函数地址 (由 usertrapret 设置)
  /*  24 */ uint64 epc;           // 保存的用户程序计数器 (SEPC)
  /*  32 */ uint64 kernel_hartid; // 保存的内核 tp 寄存器
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
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
    struct file *ofile[NOFILE];  // Open files
    struct inode *cwd;
    struct proc *parent;         // 父进程
    char name[16];               // 进程名 (调试用)
};

#endif