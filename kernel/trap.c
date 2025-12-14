// kernel/trap.c
#include "types.h"
#include "vm.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// 我们用一个 volatile 变量确保编译器不会优化掉它
extern pagetable_t kernel_pagetable;
struct spinlock tickslock;
uint ticks;
__attribute__ ((aligned (16))) char trap_stack[NCPU][4096];
void handle_exception();
extern void uartintr(void);
extern void virtio_disk_intr(void);
volatile int timer_test_interrupt_count = 0;//testuse
// 在 kernelvec.S 中，会调用 kerneltrap()
extern void kernelvec();

extern int devintr();

extern void trampoline(); 
extern void uservec();
extern void userret();

// 初始化时钟锁
void trapinit(void) {
  initlock(&tickslock, "time");
}

// S模式下的陷阱初始化
void trapinithart(void)
{
  w_stvec((uint64)kernelvec);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
}

//
// 处理来自 supervisor mode 的中断、异常或系统调用
// 由 kernelvec.S 调用
//
void kerneltrap() {
    int which_dev = 0;
    uint64 sepc = r_sepc();           // 1. 保存发生陷阱时的 PC
    uint64 sstatus = r_sstatus();     // 2. 保存状态寄存器
    uint64 scause = r_scause();

    // 检查是否来自内核态 (S-mode)
    if((sstatus & SSTATUS_SPP) == 0)
        panic("kerneltrap: not from supervisor mode");
    
    // 检查中断是否已关闭
    if(intr_get() != 0)
        panic("kerneltrap: interrupts enabled");

    // 判断是中断还是异常
    if (scause & 0x8000000000000000L) {
        // 是中断，调用 devintr 并获取返回值
        which_dev = devintr();
    } else {
        // 是异常
        handle_exception();
    }

    // --- 核心调度逻辑 ---
    // 如果是时钟中断 (which_dev == 2) 且当前有进程在运行，则让出 CPU
    if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING) {
        yield();
    }

    // yield() 返回后，说明进程再次被调度运行了
    // 必须恢复之前保存的寄存器，准备返回中断点
    w_sepc(sepc);
    w_sstatus(sstatus);
}

// --- 统一的异常处理分发函数 ---
void handle_exception() {
    uint64 scause = r_scause();
    uint64 sepc = r_sepc();
    //uint64 stval = r_stval();

    //printf("\n--- Exception Occurred ---\n");
    //printf("  scause: 0x%p\n", scause);
    //printf("  sepc (faulting instruction address): 0x%p\n", sepc);
    //printf("  stval (faulting address/info): 0x%p\n", stval);

    switch (scause) {
        case 2: // Illegal instruction
            printf("  Type: Illegal Instruction\n");
            // 在内核中，非法指令通常是致命的。
            panic("Illegal instruction");
            break;

        case 5: // Load access fault (e.g., reading from an invalid address)
            printf("  Type: Load Access Fault\n");
            panic("Load access fault");
            break;

        case 7: // Store/AMO access fault (e.g., writing to a read-only or invalid address)
            printf("  Type: Store Access Fault\n");
            panic("Store access fault");
            break;

        case 8: // Environment call from U-mode (System Call)
        case 9: // Environment call from S-mode
            printf("  Type: System Call (ecall)\n");
            // 系统调用是一个“有意为之”的异常。
            // 处理完毕后，我们需要让程序继续执行。
            // 因此，我们将 sepc + 4，指向 ecall 指令的下一条指令。
            w_sepc(sepc + 4);
            // 这里可以根据 a7 寄存器的值来分发具体的系统调用。
            // for now, we just print a message.
            printf("  System call handler finished, returning to normal execution.\n");
            break;
        
        default:
            printf("  Type: Unhandled Exception\n");
            panic("Unhandled exception");
            break;
    }
    printf("--- Exception Handling Finished ---\n\n");
}


// 时钟中断处理函数
void
clockintr()
{
  acquire(&tickslock); // 获取锁
  ticks++;             // 计数增加
  wakeup(&ticks);      // 唤醒所有在 sleep(&ticks) 的进程
  release(&tickslock); // 释放锁

  // 设置下一次中断
  w_stimecmp(r_time() + 100000); 
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

// --- 核心：用户态陷阱处理 ---
void usertrap(void) {
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // 1. 设置 stvec 为 kernelvec，因为现在我们在内核了
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // 2. 保存用户 PC (sepc)
  p->trapframe->epc = r_sepc();
  
  uint64 scause = r_scause();

  if(scause == 8) {
    // 3. 系统调用 (ecall from U-mode)
    if(p->killed)
      exit(-1);

    // sepc 指向 ecall 指令，返回时要跳过它 (+4)
    p->trapframe->epc += 4;

    // 开启中断，允许在系统调用期间被抢占
    intr_on();
    syscall();
  } else if((which_dev = devintr()) != 0) {
    // 4. 设备中断
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", scause, p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed) exit(-1);

  if(which_dev == 2) yield(); // 时间片到了

  usertrapret(); // 返回用户态
}

// --- 核心：返回用户态 ---
void usertrapret(void) {
  struct proc *p = myproc();

  // 1. 关中断
  intr_off();

  // 2. 发送 trampoline 代码的位置
  uint64 trampoline_uservec = TRAMPOLINE + (uint64)uservec - (uint64)trampoline;
  w_stvec(trampoline_uservec);

  // 3. 填充 Trapframe (供下一次从用户态进入内核时使用)
  p->trapframe->kernel_satp = r_satp();         
  p->trapframe->kernel_sp = p->kstack + PGSIZE; 
  p->trapframe->kernel_trap = (uint64)usertrap; 
  p->trapframe->kernel_hartid = r_tp();         

  // 4. 设置 SSTATUS (进入用户态前开启中断 SPIE=1, 模式 SPP=0)
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; 
  x |= SSTATUS_SPIE; 
  w_sstatus(x);

  // 5. 设置 SEPC (用户程序入口)
  w_sepc(p->trapframe->epc);

  // 6. 准备用户页表的 SATP 值
  uint64 satp = MAKE_SATP(p->pagetable);
  
  // 手动查表：看看用户页表里有没有 TRAMPOLINE 的映射
  pte_t *pte = walk(p->pagetable, TRAMPOLINE, 0);
  
  if(pte == 0) {
      panic("FATAL: Trampoline NOT mapped in User Page Table (PTE missing)!");
  }
  if((*pte & PTE_V) == 0) {
      panic("FATAL: Trampoline PTE is invalid in User Page Table!");
  }
  
  // 检查物理地址是否对齐
  uint64 pa = pte_to_pa(*pte);
  
  if(pa != 0x80004000) { // 这里填你之前 nm 看到的地址
      printf("WARNING: PA mismatch! Expected 0x80004000\n");
  }
  // ===================================

  // 7. 计算跳转目标
  uint64 fn = TRAMPOLINE + (uint64)userret - (uint64)trampoline;
  
  // 8. 真正的跳转
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}