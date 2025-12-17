#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

extern struct proc proc[NPROC];

uint64 sys_exit(void) {
  int n;
  if(argint(0, &n) < 0) return -1;
  exit(n);
  return 0; // not reached
}

uint64 sys_getpid(void) {
  return myproc()->pid;
}

uint64 sys_fork(void) {
  return fork();
}

uint64 sys_wait(void) {
  uint64 p;
  if(argaddr(0, &p) < 0) return -1;
  return wait(p);
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if(argint(0, &n) < 0) return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
    
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if(argint(0, &n) < 0) return -1;
  
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// setpriority(pid, priority)
uint64 sys_setpriority(void) {
  int pid, priority;
  if(argint(0, &pid) < 0 || argint(1, &priority) < 0)
    return -1;
  
  if(priority < 0 || priority > 10) return -1;

  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->priority = priority;
      p->wait_time = 0; // 重置防止立即发生 Aging
      release(&p->lock);
      // 如果改的是正在运行的进程，且变低了，应该让出 CPU
      if(priority < 5) yield(); 
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// getpriority(pid)
uint64 sys_getpriority(void) {
  int pid;
  if(argint(0, &pid) < 0) return -1;
  
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      int prio = p->priority;
      release(&p->lock);
      return prio;
    }
    release(&p->lock);
  }
  return -1;
}

// 打印所有进程状态
uint64 sys_ps(void) {
  struct proc *p;
  printf("\nPID\tPRIO\tSTATE\tTICKS\tWAIT\tNAME\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state != UNUSED){
      // 简单映射状态到字符串
      char *state = "???";
      if(p->state == SLEEPING) state = "SLEEP";
      if(p->state == RUNNABLE) state = "READY";
      if(p->state == RUNNING)  state = "RUN  ";
      if(p->state == ZOMBIE)   state = "ZOMB ";

      printf("%d\t%d\t%s\t%d\t%d\t%s\n", 
             p->pid, p->priority, state, p->ticks, p->wait_time, p->name);
    }
  }
  return 0;
}