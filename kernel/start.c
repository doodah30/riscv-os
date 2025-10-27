#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// entry.S jumps here in machine mode on stack0.
void
start()
{
  // --- 准备 S-mode 环境 ---
  // 读取当前的机器状态寄存器 mstatus
  unsigned long x = r_mstatus();
  // 清除 MPP (Machine Previous Privilege) 位，这是两位，所以用掩码清除
  x &= ~MSTATUS_MPP_MASK;
  // 设置 MPP 位为 S-mode (Supervisor)。
  // 这告诉 CPU：下一次执行 mret 指令时，请将权限级别从 M-mode 降至 S-mode。
  x |= MSTATUS_MPP_S;
  // 将修改后的状态写回 mstatus 寄存器
  w_mstatus(x);

  // --- 设置 S-mode 的入口点 ---
  // 将 C 语言 main 函数的地址写入 mepc (Machine Exception Program Counter) 寄存器。
  // mret 指令会把 mepc 的值加载到 PC (Program Counter)，从而实现跳转。
  w_mepc((uint64)main);

  // --- 内存与异常模型设置 ---
  // 写入 0 到 satp (Supervisor Address Translation and Protection) 寄存器，
  // 暂时关闭 M-mode 下的虚拟地址转换（分页）。
  w_satp(0);

  // 将所有异常(exceptions)和中断(interrupts)的处理权委托给 S-mode。
  // 这样，当 S-mode 发生缺页、非法指令或接收到外部中断时，
  // CPU 会直接在 S-mode 进行 trap，而不是先陷入到 M-mode。
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  
  // 开启 S-mode 对外部中断(SEIE)和时钟中断(STIE)的监听。
  // 注意：这只是“允许监听”，真正的中断总开关(SIE位)是在 S-mode 的 main 函数里开启的。
  w_sie(r_sie() | SIE_SEIE | SIE_STIE);

  // --- 硬件安全与访问权限 ---
  // 配置 PMP (Physical Memory Protection) 寄存器。
  // 这两行代码的含义是：允许 S-mode 访问全部的物理内存地址空间。
  // 如果没有这一步，S-mode 在访问内存时可能会因为 M-mode 的权限限制而失败。
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // --- 启动时钟 ---
  // 调用 timerinit 函数，配置硬件时钟以使其开始周期性地产生中断。
  timerinit();

  // --- 传递核心 ID ---
  // 读取当前 CPU 核心的硬件 ID (hart id)。
  int id = r_mhartid();
  // 将这个 ID 保存到 tp (Thread Pointer) 寄存器中。
  // 这是一个约定，让 S-mode 的代码可以随时通过读取 tp 寄存器来知道自己正在哪个核心上运行。
  w_tp(id);

  // --- 完成切换 ---
  // 执行 mret (Machine Return from Trap) 指令。
  // CPU 会执行以下原子操作：
  // 1. 读取 mstatus.MPP 的值 (S-mode)，并将当前权限级别切换到 S-mode。
  // 2. 读取 mepc 的值 (main 函数地址)，并将其加载到 PC 寄存器。
  // 3. 开始从 main 函数的第一条指令执行。
  asm volatile("mret");
}

// ask each hart to generate timer interrupts.
void
timerinit()
{
  // --- M-mode 时钟配置 ---
  // 开启 M-mode 对 S-mode 时钟中断(STIE)的监听。
  // 这是为了让 M-mode 能够将时钟中断正确地转发给 S-mode。
  w_mie(r_mie() | MIE_STIE);
  
  // 开启 sstc (Supervisor-mode timer control) 扩展。
  // 这是一个较新的 RISC-V 扩展，允许 S-mode 直接访问和设置自己的时钟比较器 stimecmp。
  w_menvcfg(r_menvcfg() | (1L << 63)); 
  
  // 允许 S-mode 访问 time 和 stimecmp 寄存器。
  w_mcounteren(r_mcounteren() | 2);
  
  // --- 预约第一次 S-mode 时钟中断 ---
  // 读取当前硬件时间 (time 寄存器)，加上一个间隔 (约 0.1 秒)，
  // 然后将这个未来的时间点写入 S-mode 的时钟比较器 (stimecmp)。
  // 当 time 的值增长到 stimecmp 的值时，就会触发第一个 S-mode 时钟中断。
  w_stimecmp(r_time() + 1000000);
}