// kernel/proc.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

uchar initcode[] = {
    0x93, 0x08, 0x00, 0x01, // li a7, 16
    0x13, 0x05, 0x10, 0x00, // li a0, 1
    0x93, 0x05, 0x00, 0x00, // li a1, 0
    0x13, 0x06, 0xc0, 0x00, // li a2, 12
    0x73, 0x00, 0x00, 0x00, // ecall

    0x93, 0x08, 0x20, 0x00, // li a7, 2
    0x73, 0x00, 0x00, 0x00, // ecall
};

struct cpu cpus[NCPU];
struct proc proc[NPROC];

struct proc *initproc;
void test_runner(void);
int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void); // 在 trap.c 或 kernelvec.S 中定义，或者是新建的

void forkret(void) {
  static int first = 1;
  release(&myproc()->lock);
  if (first) {
    // 文件系统初始化等...
    first = 0;
  }
  usertrapret(); // <--- 关键修改：进入用户空间！
}

// 初始化进程表
void procinit(void) {
    struct proc *p;
    for(int i = 0; i < NCPU; i++) {
        cpus[i].proc = 0;
        cpus[i].noff = 0;   // 必须为 0！
        cpus[i].intena = 0; // 初始为 0
    }
    initlock(&pid_lock, "nextpid");
    for(p = proc; p < &proc[NPROC]; p++) {
        initlock(&p->lock, "proc");
        p->state = UNUSED;
        // 分配内核栈 (这里简化处理，假设 kalloc 分配一页)
        // 实际 xv6 在 KSTACK 区域映射，这里如果你还没做内核栈映射，
        // 可以简单地用 kalloc() 分配物理页作为内核栈
        // char *pa = kalloc();
        // if(pa == 0) panic("kalloc");
        // uint64 va = KSTACK((int) (p - proc));
        // kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
        // p->kstack = va;
        
        // 简化版：暂不实现复杂的内核栈映射，留空或按需分配
    }
}

int cpuid() {
  int id;
  // 读取 tp 寄存器，它保存了当前核的编号
  asm volatile("mv %0, tp" : "=r" (id));
  return id;
}

// 获取当前CPU
struct cpu* mycpu(void) {
    int id = cpuid();
    struct cpu *c = &cpus[id];
    return c;
}

// 获取当前进程
struct proc* myproc(void) {
    push_off();
    struct cpu *c = mycpu();
    struct proc *p = c->proc;
    pop_off();
    return p;
}

// 分配进程ID
int allocpid() {
    int pid;
    acquire(&pid_lock);
    pid = nextpid;
    nextpid = nextpid + 1;
    release(&pid_lock);
    return pid;
}

// 在进程表中寻找一个 UNUSED 状态的进程
static struct proc* allocproc(void) {
    struct proc *p;

    for(p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if(p->state == UNUSED) {
            goto found;
        } else {
            release(&p->lock);
        }
    }
    return 0;

found:
    p->pid = allocpid();
    p->state = USED;

    // 分配陷阱帧 (如果尚未分配)
    if((p->trapframe = (struct trapframe *)kalloc()) == 0){
        release(&p->lock);
        return 0;
    }

    if((p->kstack = (uint64)kalloc()) == 0){
        kfree(p->trapframe); // 失败了要把上面分配的 trapframe 释放掉
        release(&p->lock);
        return 0;
    }

    // 初始化上下文，让它下次调度时跳转到 forkret
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64)forkret; 
    p->context.sp = p->kstack + PGSIZE; // 栈顶

    release(&p->lock); 

    return p;
}

// 调度器：在死循环中寻找 RUNNABLE 的进程并运行
void scheduler(void) {
    struct proc *p;
    struct cpu *c = mycpu();
    
    c->proc = 0;
    for(;;){
        intr_on();
        int found = 0;
        for(p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if(p->state == RUNNABLE) {
                // 切换到进程 p
                p->state = RUNNING;
                c->proc = p;
                
                // 上下文切换
                swtch(&c->context, &p->context);

                // 进程运行结束或让出 CPU，回到这里
                c->proc = 0;
                found = 1;
            }
            release(&p->lock);
        }
        
        // 如果没有进程可运行，可以稍微停顿一下避免空转过热 (wfi)
        if(found == 0) {
            intr_on();
            asm volatile("wfi");
        }
    }
}

// 放弃 CPU (yield)
void yield(void) {
    struct proc *p = myproc();
    acquire(&p->lock);
    p->state = RUNNABLE;
    sched();
    release(&p->lock);
}

// 切换回调度器
void sched(void) {
    int intena;
    struct proc *p = myproc();

    if(!holding(&p->lock))
        panic("sched p->lock");
    if(mycpu()->noff != 1)
        panic("sched locks");
    if(p->state == RUNNING)
        panic("sched running");
    if(intr_get())
        panic("sched interruptible");

    intena = mycpu()->intena;
    swtch(&p->context, &mycpu()->context);
    mycpu()->intena = intena;
}

// 休眠
void sleep(void *chan, struct spinlock *lk) {
    struct proc *p = myproc();
    
    // 必须持有 p->lock 才能修改 p->state
    if(lk != &p->lock){
        acquire(&p->lock);
        release(lk);
    }

    p->chan = chan;
    p->state = SLEEPING;

    sched(); // 切换

    // 醒来后
    p->chan = 0;
    if(lk != &p->lock){
        release(&p->lock);
        acquire(lk);
    }
}

// 唤醒
void wakeup(void *chan) {
    struct proc *p;

    for(p = proc; p < &proc[NPROC]; p++) {
        if(p != myproc()){
            acquire(&p->lock);
            if(p->state == SLEEPING && p->chan == chan) {
                p->state = RUNNABLE;
            }
            release(&p->lock);
        }
    }
}

void userinit(void) {
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // 1. 创建页表 (此时里面已经有了 Trampoline)
  p->pagetable = uvmcreate();
  if(p->pagetable == 0) panic("userinit: uvmcreate");

  // 2. 映射 initcode
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // 3. === 关键修复：映射 Trapframe ===
  // 将内核分配的 p->trapframe (物理地址/内核虚地址) 
  // 映射到用户空间的固定高地址 TRAPFRAME
  if(mappages(p->pagetable, TRAPFRAME, PGSIZE, 
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0) {
      panic("userinit: map trapframe");
  }

  // 4. 设置上下文
  p->trapframe->epc = 0;      // 用户程序入口
  p->trapframe->sp = PGSIZE;  // 用户栈顶

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->state = RUNNABLE;

  // 此时不需要 release(&p->lock)，因为 allocproc 已经释放了
}

void exit(int status) {
  struct proc *p = myproc();

  //if(p == initproc)
  //  panic("init exiting");

  // 打印一条日志方便调试
  printf("PID %d exited with status %d\n", p->pid, status);

  // 获取进程锁
  acquire(&p->lock);

  // 标记退出状态
  p->xstate = status;
  
  // 变更为僵尸状态，不再会被调度运行
  p->state = ZOMBIE;

  // 调度器切换到其他进程
  // 注意：sched() 会释放 p->lock，并在返回时重新获取
  // 但由于状态是 ZOMBIE，sched() 永远不会返回这里
  sched();
  
  panic("zombie exit");
}

// 增长或缩小进程内存 (供 sys_sbrk 调用)
int growproc(int n) {
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// 创建当前进程的副本
int fork(void) {
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // 1. 分配新进程
  if((np = allocproc()) == 0){
    return -1;
  }

  // 2. 复制用户内存
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    // freeproc(np); // 暂时简化，不处理释放
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;
  np->parent = p;
  // 3. 复制 Trapframe (寄存器状态)
  *(np->trapframe) = *(p->trapframe);

  // 4. fork 返回值：子进程返回 0
  np->trapframe->a0 = 0;

  // 5. 复制名字
  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  // 6. 设置子进程状态为可运行
  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// 等待子进程退出
int wait(uint64 addr) {
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&p->lock); // 获取自己的锁，准备睡觉或操作

  for(;;){
    // 扫描进程表，看看有没有我的子进程
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){ // 这里需要你在 struct proc 里加一个 parent 指针！
        // 暂时为了编译通过，我们假设没有 parent 指针，用简单的逻辑演示
        // 实际上你需要在 struct proc 里加 struct proc *parent;
        // 并在 fork 时候 np->parent = p;
        // 这里简化：假设所有 ZOMBIE 都是我的孩子 (这在多进程下是错的，但在简单测试下能跑)
        havekids = 1;
        
        acquire(&np->lock);
        if(np->state == ZOMBIE){
          // 找到一个僵尸子进程，回收它
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate, sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&p->lock);
            return -1;
          }
          
          // 清理进程表槽位 (freeproc 的逻辑)
          np->state = UNUSED;
          np->pid = 0;
          np->parent = 0;
          np->killed = 0;
          np->xstate = 0;
          
          release(&np->lock);
          release(&p->lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // 如果没有子进程，或者被杀死了
    if(!havekids || p->killed){
      release(&p->lock);
      return -1;
    }

    // 等待子进程退出 (sleep)
    // 这里需要一个等待通道，通常用 p 本身的地址
    sleep(p, &p->lock); 
  }
}