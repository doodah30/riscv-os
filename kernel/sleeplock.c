#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"

void initsleeplock(struct sleeplock *lk, char *name) {
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

void acquiresleep(struct sleeplock *lk) {
  acquire(&lk->lk);
  while (lk->locked) {
    sleep(lk, &lk->lk);
  }
  lk->locked = 1;
  struct proc *p = myproc();
  if(p) {
      lk->pid = p->pid;
  } else {
      lk->pid = 0; // 如果是内核启动阶段（无进程），设为 0
  }
  release(&lk->lk);
}

void releasesleep(struct sleeplock *lk) {
  acquire(&lk->lk);
  lk->locked = 0;
  struct proc *p = myproc();
  if(p) {
      lk->pid = 0;
  }
  wakeup(lk);
  release(&lk->lk);
}

int holdingsleep(struct sleeplock *lk) {
  int r;
  acquire(&lk->lk);
  struct proc *p = myproc();
  r = lk->locked && (p ? (lk->pid == p->pid) : (lk->pid == 0));
  release(&lk->lk);
  return r;
}