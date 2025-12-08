// kernel/proc.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];
struct proc proc[NPROC];

struct proc *initproc;
void test_runner(void);
int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void); // 在 trap.c 或 kernelvec.S 中定义，或者是新建的

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

void forkret(void) {
  static int first = 1;

  // 释放进程锁
  // 调度器在该进程运行前获取了锁，新进程必须释放它
  release(&myproc()->lock);

  if (first) {
    // 这里将来可以放文件系统初始化代码
    first = 0;
  }
  
  // 对于真正的用户进程，这里会调用 usertrapret() 返回用户空间
  // 但对于我们的内核线程测试，函数会直接返回，或者由 ra 寄存器跳转到 test_runner
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
    p->state = RUNNABLE;
    p->context.ra = (uint64)test_runner; // 设置入口为 test_runner
    p->context.sp = p->kstack + PGSIZE;
    
    printf("Userinit: Created test_runner process (PID %d)\n", p->pid);
}

int create_kernel_process(void (*entry)(void)) {
    struct proc *p;
    if((p = allocproc()) == 0){
        return -1;
    }
    
    p->state = RUNNABLE;
    // 设置上下文，让它下次调度时执行 entry 函数
    p->context.ra = (uint64)entry;
    p->context.sp = p->kstack + PGSIZE;
    
    return p->pid;
}

// --- 手册任务6：进程状态调试 ---
void debug_proc_table(void) {
    struct proc *p;
    printf("\n=== Process Table ===\n");
    for(p = proc; p < &proc[NPROC]; p++){
        if(p->state != UNUSED){
            const char *state_name;
            switch(p->state){
                case USED: state_name = "USED"; break;
                case SLEEPING: state_name = "SLEEPING"; break;
                case RUNNABLE: state_name = "RUNNABLE"; break;
                case RUNNING: state_name = "RUNNING"; break;
                case ZOMBIE: state_name = "ZOMBIE"; break;
                default: state_name = "UNKNOWN"; break;
            }
            printf("PID: %d | State: %s\n", p->pid, state_name);
        }
    }
    printf("=====================\n");
}

// --- 测试1：进程创建测试 (手册 P30) ---
void simple_task(void) {
    release(&myproc()->lock);
    intr_on();
    printf("Simple task running (PID %d)\n", myproc()->pid);
    while(1) {
        // 保持运行，让调度器有机会切走
        // 为了避免死循环占满输出，加点延时
         for(volatile int i=0; i<1000000; i++);
         // yield(); // 可选：主动让出
    }
}

void test_process_creation(void) {
    printf("\n[Test] Process Creation...\n");
    int pid1 = create_kernel_process(simple_task);
    int pid2 = create_kernel_process(simple_task);
    
    if(pid1 > 0 && pid2 > 0) {
        printf("SUCCESS: Created processes PID %d and %d\n", pid1, pid2);
    } else {
        printf("FAIL: Process creation failed\n");
    }
    
    debug_proc_table(); // 打印状态验证
}


// --- 测试2：同步机制测试 (手册 P31) ---
// 经典的生产者-消费者模型

struct {
    struct spinlock lock;
    int buffer;
    int data_ready; // 0: 空, 1: 满
} shared_chan;

void producer_task(void) {
    release(&myproc()->lock);
    intr_on();
    for(int i = 1; i <= 3; i++) {
        acquire(&shared_chan.lock);
        while(shared_chan.data_ready == 1) {
            // 缓冲区满，等待消费者取走
            sleep(&shared_chan, &shared_chan.lock);
        }
        
        // 生产数据
        shared_chan.buffer = i * 10;
        shared_chan.data_ready = 1;
        printf("Producer: Produced %d\n", shared_chan.buffer);
        
        wakeup(&shared_chan); // 唤醒消费者
        release(&shared_chan.lock);
    }
    printf("Producer finished.\n");
    while(1); // 结束
}

void consumer_task(void) {
    release(&myproc()->lock);
    intr_on();
    for(int i = 1; i <= 3; i++) {
        acquire(&shared_chan.lock);
        while(shared_chan.data_ready == 0) {
            // 缓冲区空，等待生产者生产
            sleep(&shared_chan, &shared_chan.lock);
        }
        
        // 消费数据
        int data = shared_chan.buffer;
        shared_chan.data_ready = 0;
        printf("Consumer: Consumed %d\n", data);
        
        wakeup(&shared_chan); // 唤醒生产者
        release(&shared_chan.lock);
    }
    printf("Consumer finished.\n");
    while(1); // 结束
}

void test_synchronization(void) {
    printf("\n[Test] Synchronization (Producer/Consumer)...\n");
    initlock(&shared_chan.lock, "shared");
    shared_chan.data_ready = 0;
    
    create_kernel_process(consumer_task); // 先启动消费者，它应该会 sleep
    create_kernel_process(producer_task);
}

// --- 测试3：调度器测试 (手册 P31) ---
// 手册建议创建 CPU 密集型任务并观察
void cpu_intensive_task(void) {
    release(&myproc()->lock);
    intr_on();
    int pid = myproc()->pid;
    for(int i = 0; i < 5; i++) {
        printf("Task PID %d running iteration %d\n", pid, i);
        // 模拟耗时
        for(volatile int k = 0; k < 10000000; k++);
    }
    printf("Task PID %d finished.\n", pid);
    while(1);
}

void test_scheduler_manual(void) {
    printf("\n[Test] Scheduler...\n");
    for(int i = 0; i < 3; i++) {
        create_kernel_process(cpu_intensive_task);
    }
}

void test_runner(void) {
    release(&myproc()->lock);
    printf("\n=== Starting Experiment 5 Tests ===\n");

    // 1. 测试进程创建和查看状态
    test_process_creation();

    // 2. 测试同步 (休眠/唤醒)
    // 注意：因为我们没有实现 wait() 系统调用，
    // 这里启动后，测试进程会和当前进程并发运行
    test_synchronization();

    // 3. 测试调度公平性
    test_scheduler_manual();

    printf("\n=== All Tests Launched ===\n");
    printf("System will now schedule between these tasks forever.\n");
    
    while(1) {
        // 保持运行，防止 PID 1 退出导致 panic (如果实现了 exit 逻辑)
        // 也可以在这里定期打印 proc table
        for(volatile int i=0; i<50000000; i++);
        // debug_proc_table(); 
    }
}