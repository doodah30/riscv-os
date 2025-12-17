// user/init.c
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main() {
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  printf("init: starting fstest...\n");
  
  pid = fork();
  if(pid < 0){
    printf("init: fork failed\n");
    exit(1);
  }
  
  if(pid == 0){
    // 子进程执行测试
    //char *argv[] = { "fstest", 0 };
    //exec("fstest", argv);
    // === 执行 systest ===
    //char *argv[] = { "systest", 0 };
    //exec("systest", argv);
    // ===========================
    // === 执行 prio_test ===
    //char *argv[] = { "prio_test", 0 };
    //exec("prio_test", argv);
    // ===========================
    // === 执行 prio_test ===
    char *argv[] = { "same_prio_test", 0 };
    exec("same_prio_test", argv);
    // ===========================
    printf("init: exec fstest failed\n");
    exit(1);
  }

  // 父进程等待
  for(;;){
    wpid = wait((int *) 0);
    if(wpid == pid){
      // 测试结束
      printf("init: test finished.\n");
      // 可以在这里死循环，或者关机
      for(;;);
    }
  }
}