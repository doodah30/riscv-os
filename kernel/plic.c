// kernel/plic.c

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void
plicinit(void)
{
  // 为UART和virtio磁盘中断设置一个非零的优先级 (否则它们会被禁用)
  *(uint32*)(PLIC + UART0_IRQ * 4) = 1;
  *(uint32*)(PLIC + VIRTIO0_IRQ * 4) = 1;
}

void
plicinithart(void)
{
  // --- 错误修复在这里 ---
  // 我们的内核目前是单核运行，所以 hart id 总是 0.
  // 我们将 cpuid() 调用替换为硬编码的 0.
  int hart = 0; 
  
  // 为这个核心的 Supervisor mode 开启 UART 和 virtio 磁盘中断
  *(uint32*)PLIC_SENABLE(hart) = (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);

  // 设置这个核心的 Supervisor mode 中断优先级阈值为 0
  // (任何优先级 > 0 的中断都会被处理)
  *(uint32*)PLIC_SPRIORITY(hart) = 0;
}

// 查询 PLIC，是哪个设备触发了中断
int
plic_claim(void)
{
  // --- 同样修复这里 ---
  int hart = 0;
  int irq = *(uint32*)PLIC_SCLAIM(hart);
  return irq;
}

// 通知 PLIC，我们已经处理完这个中断了
void
plic_complete(int irq)
{
  // --- 同样修复这里 ---
  int hart = 0;
  *(uint32*)PLIC_SCLAIM(hart) = irq;
}