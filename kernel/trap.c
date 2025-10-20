// kernel/trap.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

// 我们用一个 volatile 变量确保编译器不会优化掉它
volatile uint ticks;

extern void uartintr(void);
extern void virtio_disk_intr(void);

// 在 kernelvec.S 中，会调用 kerneltrap()
void kernelvec();

extern int devintr();

// S模式下的陷阱初始化
void trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// 处理来自 supervisor mode 的中断、异常或系统调用
// 由 kernelvec.S 调用
//
void
kerneltrap()
{
  // devintr() 会处理中断并返回
  devintr();
}

// 时钟中断处理函数
void
clockintr()
{
  // 这是一个简单的原子操作，对于我们目前的单线程内核足够了
  ticks++;

  // 每隔一段时间打印一次，证明时钟在工作
  // 注意：频繁打印会极大地拖慢系统
  if (ticks % 100 == 0) {
      printf("tick\n");
  }
}

// 检查是外部中断还是软件中断，并处理它
// 返回 2 代表是时钟中断,
// 返回 1 代表是其他设备,
// 返回 0 代表无法识别.
int
devintr()
{
  uint64 scause = r_scause();

  if(scause == 0x8000000000000009L){//其实暂时不会进入此分支
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // timer interrupt.
    clockintr();
    return 2;
  } else {
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
    return 0;
  }
}