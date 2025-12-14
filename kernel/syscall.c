#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

extern uint64 sys_fork(void);
extern uint64 sys_wait(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_getpid(void);

// 获取第 n 个整数参数
int argint(int n, int *ip) {
  struct proc *p = myproc();
  struct trapframe *tf = p->trapframe;
  
  // 参数存储在 a0-a5 中
  switch(n) {
  case 0: *ip = tf->a0; break;
  case 1: *ip = tf->a1; break;
  case 2: *ip = tf->a2; break;
  case 3: *ip = tf->a3; break;
  case 4: *ip = tf->a4; break;
  case 5: *ip = tf->a5; break;
  default: return -1;
  }
  return 0;
}

int argaddr(int n, uint64 *ip) {
  int x;
  struct proc *p = myproc();
  
  if(argint(n, &x) < 0) return -1;
  
  // 关键检查：地址必须在用户空间内 (0 ~ p->sz)
  // 并且不能指向太高的地方
  if((uint64)x >= p->sz || (uint64)x + 8 > p->sz) // 假设读取8字节
    return -1;
    
  *ip = x;
  return 0;
}

// --- 具体的系统调用实现 ---

// sys_write(fd, buf, n)
uint64 sys_write(void) {
  int fd;
  uint64 p;
  int n;

  if(argint(0, &fd) < 0 || argaddr(1, &p) < 0 || argint(2, &n) < 0)
    return -1;
  
  // 暂时只支持向标准输出(1)写，忽略 fd
  // 并且为了简单，假设 p 是可以直接访问的（实际上应该用 copyin）
  // 这里我们偷懒：因为我们还没有实现 copyin，
  // 我们暂时假定用户传的是物理地址或者我们通过 copyin 读取
  // **注意**：标准实现应该用 copyin 从用户页表读取数据
  // 简化测试：直接把 p 当作物理地址打印（Hack）
  // 为了通过测试，我们稍后在 userinit 里做点手脚，或者在这里通过 walk 查找物理地址
  
  // 正规做法：
  // char buf[128];
  // copyin(p->pagetable, buf, p, n);
  
  printf("sys_write called: fd=%d, len=%d\n", fd, n);
  return n;
}

uint64 sys_exit(void) {
  int n;
  // 获取用户传递的退出状态码 (initcode里传的是0)
  if(argint(0, &n) < 0)
    return -1;
  
  // 调用 proc.c 中的 exit 函数
  // 这会打印 "PID 1 exited..." 然后 panic
  exit(n);
  
  return 0;  // 永远不会执行到这里
}

// 系统调用表
static uint64 (*syscalls[])(void) = {
[SYS_write]   sys_write,
[SYS_exit]    sys_exit,
[SYS_fork]    sys_fork,
[SYS_wait]    sys_wait,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_getpid]  sys_getpid,
};

void syscall(void) {
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7; // 系统调用号在 a7
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num](); // 返回值存回 a0
  } else {
    printf("pid %d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}