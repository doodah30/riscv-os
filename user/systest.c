#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define assert(x) if(!(x)) { printf("FAILED: %s:%d\n", __FILE__, __LINE__); exit(1); }

void test_basic() {
  printf("[TEST] Basic Syscalls (getpid, fork, wait, exit)...\n");
  
  int pid = getpid();
  assert(pid > 0);
  printf("  Current PID: %d\n", pid);

  int child = fork();
  if(child < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if(child == 0) {
    // 子进程
    printf("  Child exiting with magic status 88...\n");
    exit(88); // 正常退出
  } else {
    // 父进程
    int status;
    int wpid = wait(&status);
    assert(wpid == child);
    assert(status == 88);
    printf("  Parent waited for child %d, status %d\n", wpid, status);
  }
  printf("PASSED\n");
}

void test_args() {
  printf("[TEST] Parameter Passing (write)...\n");
  
  // 测试向标准输出写不同长度的数据
  char *str = "Hello";
  int n = write(1, str, 5);
  assert(n == 5);
  printf("\n"); // 换行

  // 测试边界情况：长度为0
  n = write(1, str, 0);
  assert(n == 0);

  // 测试写到无效文件描述符
  n = write(999, str, 5);
  assert(n == -1);

  printf("PASSED\n");
}

void test_security() {
  printf("[TEST] Security (Invalid Pointers)...\n");
  
  // 1. 尝试向内核读取数据 (传入空指针或极大地址)
  // 内核不应该 Panic，而应该返回 -1 (错误)
  
  // 尝试向 NULL 地址写入 (fd 1 是 console)
  // 注意：在某些实现中 0 地址可能是有效的 (代码段)，
  // 但我们试试写入一个肯定非法的超大地址 (内核空间)
  
  uint64 kernel_addr = 0x80000000; // KERNBASE
  int n = write(1, (void*)kernel_addr, 1);
  
  if (n != -1) {
      printf("  FAILED: Write to kernel address succeeded? (ret=%d)\n", n);
      printf("  Security hole detected!\n");
      exit(1);
  } else {
      printf("  Write to kernel addr handled correctly (ret=-1)\n");
  }

  // 2. 尝试读取 (read) 到内核地址
  // 假设 fd 0 是 console
  n = read(0, (void*)kernel_addr, 1);
  if (n != -1) {
      printf("  FAILED: Read to kernel address succeeded? (ret=%d)\n", n);
      exit(1);
  } else {
      printf("  Read to kernel addr handled correctly (ret=-1)\n");
  }

  printf("PASSED\n");
}

void test_performance() {
  printf("[TEST] System Call Performance...\n");
  
  int start = uptime();
  
  // 执行大量轻量级系统调用 (getpid)
  int loops = 100000;
  for(int i = 0; i < loops; i++) {
      getpid();
  }
  
  int end = uptime();
  int duration = end - start;
  
  printf("  %d getpid() calls took %d ms\n", loops, duration);
  
  if(duration > 0) {
      // 计算每次调用的微秒数
      // duration (ms) * 1000 / loops
      int avg_us_x10 = (duration * 10000) / loops;
      printf("  Average: %d.%d us per call\n", avg_us_x10/10, avg_us_x10%10);
  }
  
  printf("PASSED\n");
}

int main(int argc, char *argv[]) {
  printf("=== Starting Lab 6 System Call Tests ===\n");
  
  test_basic();
  test_args();
  test_security();
  test_performance();
  
  printf("=== All Lab 6 Tests Passed! ===\n");
  exit(0);
}