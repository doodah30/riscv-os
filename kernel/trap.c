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
  // 读取 scause 寄存器，它记录了这次 trap 的原因。
  uint64 scause = r_scause();
  // --- 判断中断类型 ---

  // scause 最高位为 1 表示是中断，最低几位是中断号。
  // 中断号 9 代表 Supervisor External Interrupt (来自 PLIC 的外部中断)。
  if(scause == 0x8000000000000009L){//其实暂时不会进入这个分支
    // 调用 plic_claim() 查询是哪个外部设备（如 UART）触发了中断。
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();// 调用 UART 的中断处理函数
    } else if(irq == VIRTIO0_IRQ){// ... 其他设备
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // 通知 PLIC，这个中断已经处理完毕，可以接收来自该设备的下一个中断了。
    if(irq)
      plic_complete(irq);
    return 1; // 返回 1 代表是外部中断

  // 中断号 5 代表 Supervisor Timer Interrupt (S-mode 时钟中断)。
  } else if(scause == 0x8000000000000005L){
    // 调用我们定义的时钟中断处理函数 clockintr()。
    clockintr();
    return 2; // 返回 2 代表是时钟中断
    
  } else {
    // 如果 scause 的值不是我们能识别的中断号，
    // 说明发生了我们尚未处理的异常（如缺页、非法指令）。
    // 打印所有调试信息，然后调用 panic() 停机，防止系统继续在错误状态下运行。
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
    return 0;
  }
}