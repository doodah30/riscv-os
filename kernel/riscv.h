// riscv.h - minimal RISC-V / Sv39 helpers and PTE macros
#ifndef RISCV_H
#define RISCV_H

#include "types.h"
#include <stdint.h>
#include <stddef.h>

#define PGSIZE 4096UL
#define PGSHIFT 12UL

typedef uint64_t pte_t;

// PTE flag bits
#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_G (1UL << 5)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)

// Sv39 specifics: 9 bits per level
static inline uint64_t vpn_index(uint64_t va, int level) {
    // level: 0 => VPN[0] (low), 1 => VPN[1], 2 => VPN[2] (high)
    return (va >> (PGSHIFT + 9 * level)) & 0x1FFUL;
}

// PTE <-> physical conversions (PTE stores PPN at bits >= 10)
static inline uint64_t pte_to_pa(pte_t pte) {
    return ((pte >> 10) << PGSHIFT);
}
static inline pte_t pa_to_pte(uint64_t pa, uint64_t flags) {
    return (((pa >> PGSHIFT) << 10) | (flags));
}

// Simple PA <-> kernel VA conversion macros.
// DEFAULT: identity (suitable for early boot / identity-mapped kernels).
// When you enable a kernel direct-map at KERNBASE, change these, e.g.:
//   #define PA2VA(pa) ((void *)((pa) + KERNBASE))
//   #define VA2PA(va) ((uint64_t)(va) - KERNBASE)
#ifndef PA2VA
#define PA2VA(pa) ((void *)(pa))
#endif
#ifndef VA2PA
#define VA2PA(va) ((uint64_t)(va))
#endif

// sfence.vma wrapper to flush TLB (for RISC-V)
static inline void sfence_vma(void) {
    // flush all entries (no ASID here)
    asm volatile("sfence.vma" ::: "memory");
}


//------------------------------------
// ---------- 核心与权限状态 -----------
//------------------------------------

// 读取 mhartid (Machine Hart ID) 寄存器，获取当前正在执行代码的 CPU 核心（hart）的唯一硬件ID。
static inline uint64
r_mhartid()
{
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r" (x) );
  return x;
}

// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11) //定义一个掩码来操作 mstatus 寄存器中的 MPP位。
#define MSTATUS_MPP_M (3L << 11)//定义了 MPP 为 M-mode 的值。
#define MSTATUS_MPP_S (1L << 11)//定义了 MPP 为 S-mode 的值。
#define MSTATUS_MPP_U (0L << 11)//定义了 MPP 为 U-mode 的值。
//读/写 mstatus (Machine Status) 寄存器。
//这是 M-mode 下最重要的状态寄存器，像一个“仪表盘”，
//控制着全局中断使能、前一个权限级别 (MPP)、浮点单元状态等。
//我们在 start.c 中用它来指定 mret 后要进入 S-mode。
static inline uint64
r_mstatus()
{
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r" (x) );
  return x;
}

static inline void 
w_mstatus(uint64 x)
{
  asm volatile("csrw mstatus, %0" : : "r" (x));
}
//定义了 sstatus 寄存器中单个控制位的位置。
#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

static inline uint64
r_sstatus()
{
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

static inline void 
w_sstatus(uint64 x)
{
  asm volatile("csrw sstatus, %0" : : "r" (x));
}

//------------------------------------
// --------- Trap 处理与返回 ----------
//------------------------------------

// 写入 mepc寄存器。
// mepc 存放的是 M-mode trap 处理完毕后，mret 指令应该返回到的地址。
// 在 start.c 中把 main 函数的地址写入它，从而实现从 M-mode 到 S-mode main 函数的跳转。
static inline void 
w_mepc(uint64 x)
{
  asm volatile("csrw mepc, %0" : : "r" (x));
}

// 读/写 sepc (Supervisor Exception Program Counter) 寄存器。
// 与 mepc 类似，它存放的是 S-mode trap 处理完毕后，sret 指令应该返回到的地址。
// 当发生中断时，CPU 会自动将被中断的指令地址存入 sepc。
static inline void 
w_sepc(uint64 x)
{
  asm volatile("csrw sepc, %0" : : "r" (x));
}

static inline uint64
r_sepc()
{
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}

//读取 scause (Supervisor Cause) 寄存器。
//当 trap 发生时，CPU 会把原因代码写入这个寄存器。
//devintr 函数就是通过读取它来判断是时钟中断、外部中断还是其他异常。
static inline uint64
r_scause()
{
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x) );
  return x;
}

// 读取 stval (Supervisor Trap Value) 寄存器。
//它提供了与 trap 相关的附加信息。
//例如，如果发生了缺页异常，stval 会存放导致异常的那个虚拟地址。
static inline uint64
r_stval()
{
  uint64 x;
  asm volatile("csrr %0, stval" : "=r" (x) );
  return x;
}

//------------------------------------
// ------------ 中断控制  -------------
//------------------------------------

// 读/写 medeleg (Machine Exception Delegation) 寄存器。
//这是一个位掩码，M-mode 通过它来决定哪些异常 (Exceptions) 不需要自己处理，而是直接“委托”给 S-mode。
static inline uint64
r_medeleg()
{
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r" (x) );
  return x;
}

static inline void 
w_medeleg(uint64 x)
{
  asm volatile("csrw medeleg, %0" : : "r" (x));
}

// 读/写 mideleg (Machine Interrupt Delegation) 寄存器。
//它用来决定哪些中断可以直接委托给 S-mode 处理。
//我们在 start.c 中把它们都设置为 0xffff，意味着几乎所有事情都交给 S-mode。
static inline uint64
r_mideleg()
{
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r" (x) );
  return x;
}

static inline void 
w_mideleg(uint64 x)
{
  asm volatile("csrw mideleg, %0" : : "r" (x));
}

//读/写 sip (Supervisor Interrupt Pending) 寄存器。
//它显示当前有哪些 S-mode 中断正在等待处理。
//比如，S-mode 时钟中断发生时，sip 的 STIP 位会被置位。
//我们在 devintr 中处理完时钟中断后，需要手动清除这一位。
static inline uint64
r_sip()
{
  uint64 x;
  asm volatile("csrr %0, sip" : "=r" (x) );
  return x;
}

static inline void 
w_sip(uint64 x)
{
  asm volatile("csrw sip, %0" : : "r" (x));
}

//读/写 sie (Supervisor Interrupt Enable) 寄存器。
//这是一个中断使能掩码，用来分别控制是否允许 S-mode 响应外部中断 (SEIE)、时钟中断 (STIE) 等。
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
static inline uint64
r_sie()
{
  uint64 x;
  asm volatile("csrr %0, sie" : "=r" (x) );
  return x;
}

static inline void 
w_sie(uint64 x)
{
  asm volatile("csrw sie, %0" : : "r" (x));
}

//读/写 mie (Machine Interrupt Enable) 寄存器。
//这是 M-mode 的中断使能掩码，我们在 start.c 中用它来开启对 S-mode 时钟中断的监听。
#define MIE_STIE (1L << 5)  //mie 寄存器中，控制 S-mode 时钟中断的使能位在第 5 位。
static inline uint64
r_mie()
{
  uint64 x;
  asm volatile("csrr %0, mie" : "=r" (x) );
  return x;
}

static inline void 
w_mie(uint64 x)
{
  asm volatile("csrw mie, %0" : : "r" (x));
}

//通过修改 sstatus 寄存器的 SIE (Supervisor Interrupt Enable) 位
//全局性地开启或关闭所有 S-mode 中断。这是中断控制的“总闸门”。
static inline void
intr_on()
{
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// disable device interrupts
static inline void
intr_off()
{
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// are device interrupts enabled?
static inline int
intr_get()
{
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

//------------------------------------
// ----------- 时钟与计数器 ------------
//------------------------------------

//读取 time 寄存器，这是一个 64 位的硬件时钟计数器，
//从系统上电开始就以一个固定的频率（由 QEMU 或硬件决定）自增。
static inline uint64
r_time()
{
  uint64 x;
  asm volatile("csrr %0, time" : "=r" (x) );
  return x;
}

//读/写 stimecmp寄存器。这是一个由 S-mode 控制的“闹钟”。
//我们在 timerinit 和 kerneltrap 中向它写入一个未来的 time 值。
//当 time 寄存器的值增长到大于等于 stimecmp 的值时，就会触发一次 S-mode 时钟中断。
static inline uint64
r_stimecmp()
{
  uint64 x;
  // asm volatile("csrr %0, stimecmp" : "=r" (x) );
  asm volatile("csrr %0, 0x14d" : "=r" (x) );
  return x;
}

static inline void 
w_stimecmp(uint64 x)
{
  // asm volatile("csrw stimecmp, %0" : : "r" (x));
  asm volatile("csrw 0x14d, %0" : : "r" (x));
}

//读/写 menvcfg (Machine Environment Configuration) 寄存器。
//这是一个较新的配置寄存器，我们在 start.c 中用它来开启 sstc 扩展，从而允许 S-mode 访问 stimecmp。
static inline uint64
r_menvcfg()
{
  uint64 x;
  // asm volatile("csrr %0, menvcfg" : "=r" (x) );
  asm volatile("csrr %0, 0x30a" : "=r" (x) );
  return x;
}

static inline void 
w_menvcfg(uint64 x)
{
  // asm volatile("csrw menvcfg, %0" : : "r" (x));
  asm volatile("csrw 0x30a, %0" : : "r" (x));
}

//读/写 mcounteren (Machine Counter-Enable) 寄存器。
//M-mode 通过它来授权 S-mode 访问某些计数器，比如 time 寄存器。
static inline void 
w_mcounteren(uint64 x)
{
  asm volatile("csrw mcounteren, %0" : : "r" (x));
}

static inline uint64
r_mcounteren()
{
  uint64 x;
  asm volatile("csrr %0, mcounteren" : "=r" (x) );
  return x;
}
//------------------------------------
// ---------- 内存管理与保护  -----------
//------------------------------------
//写 pmpcfg0 和 pmpaddr0 寄存器。
//它们是 PMP (Physical Memory Protection) 机制的一部分。
//M-mode 通过它们来定义一片物理内存区域，并设置 S-mode 对这片区域的访问权限（读、写、执行）。
//我们在 start.c 中用它来赋予 S-mode 对全部物理内存的访问权。
static inline void
w_pmpcfg0(uint64 x)
{
  asm volatile("csrw pmpcfg0, %0" : : "r" (x));
}

static inline void
w_pmpaddr0(uint64 x)
{
  asm volatile("csrw pmpaddr0, %0" : : "r" (x));
}
//读/写 stvec寄存器。它存放着 S-mode 的统一 trap 处理入口函数的地址。
//我们在 trapinithart 中把我们写的汇编函数 kernelvec 的地址写入它。
static inline void 
w_stvec(uint64 x)
{
  asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline uint64
r_stvec()
{
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}
// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)//Sv39 分页模式在 satp 寄存器中的编码值。

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

//读/写 satp (Supervisor Address Translation and Protection) 寄存器。
//这是开启分页机制的核心。它包含了根页表的物理地址和地址转换模式（如 Sv39）。
//当 main 调用 kvminithart 时，就是通过写 satp 来启动虚拟内存的。
static inline void 
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64
r_satp()
{
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x) );
  return x;
}
//------------------------------------
// ---------- 线程指针与其他  -----------
//------------------------------------

//读取 sp (Stack Pointer)，获取当前栈顶地址。
static inline uint64
r_sp()
{
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}

//读/写 tp (Thread Pointer)。
//xv6 有一个巧妙的约定，即用 tp 寄存器来存放当前核心的 hart id。
//这样，在任何 C 代码中，都可以通过 cpuid() 函数快速知道自己正在哪个 CPU 核心上运行。
static inline uint64
r_tp()
{
  uint64 x;
  asm volatile("mv %0, tp" : "=r" (x) );
  return x;
}

static inline void 
w_tp(uint64 x)
{
  asm volatile("mv tp, %0" : : "r" (x));
}

static inline uint64
r_ra()
{
  uint64 x;
  asm volatile("mv %0, ra" : "=r" (x) );
  return x;
}
#endif // RISCV_H