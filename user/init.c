// user/init.c
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main() {
  int pid, wpid;

  // 打开控制台 (fd 0, 1, 2)
  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  //printf("init: starting...\n");
  
  // 这里暂时简化，只打印一句话，证明加载成功
  // 以后我们会在这里 fork 并 exec "sh"
  const char *msg = "I am the real INIT process from disk!\n";
  write(1, msg, 38);

  for(;;){
      // 死循环，防止退出
  }
}