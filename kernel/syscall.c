#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_wait(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_getpid(void);

extern uint64 sys_write(void); 
extern uint64 sys_read(void);
extern uint64 sys_fstat(void);
extern uint64 sys_open(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
//extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_uptime(void);

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

int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz) // both tests needed, in case of overflow
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  if(copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

// 系统调用表
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
//[SYS_pipe]    sys_pipe,    // 假设你有 pipe
[SYS_read]    sys_read,
//[SYS_kill]    sys_kill,    // 假设你有 kill
[SYS_exec]    sys_exec,    // 假设你有 exec
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,  // 假设你有 uptime
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
//[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
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