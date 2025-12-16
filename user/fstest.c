#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// 简单的 assert 实现
#define assert(x) if(!(x)) { printf("FAILED: %s:%d\n", __FILE__, __LINE__); exit(1); }

void test_integrity() {
  printf("[TEST] Filesystem Integrity...\n");
  
  int fd;
  char *fname = "test_file";
  char *str = "Hello, OS!";
  char buf[32];

  // 1. 创建并写入
  // O_CREATE | O_RDWR: 创建并以读写模式打开。
  fd = open(fname, O_CREATE | O_RDWR);
  assert(fd >= 0); // 确保 fd 分配成功 (filealloc 工作正常)
  
  // 写入字符串。write 返回实际写入的字节数，必须等于字符串长度。
  int n = write(fd, str, strlen(str));
  assert(n == strlen(str));
  close(fd); // 关闭，释放 inode 锁

  // 2. 重新打开并读取
  // 这里是为了验证：数据真的存进去了，而且关了文件再开还能读到。
  fd = open(fname, O_RDONLY);
  assert(fd >= 0);
  
  memset(buf, 0, sizeof(buf));
  // 验证读出的数据。
  n = read(fd, buf, sizeof(buf));
  assert(n == strlen(str)); // 长度必须对
  assert(strcmp(buf, str) == 0); // 内容必须对
  close(fd);

  // 3. 删除文件
  // 原理：sys_unlink。它会从当前目录的数据块中抹去 "test_file" 这个条目。
  // 同时，inode 的 nlink 减 1。如果减到 0，内核会回收 inode 和数据块 (itrunc)。
  assert(unlink(fname) == 0);
  
  // 再次尝试打开。
  // 预期：必须失败 (fd < 0)。如果还能打开，说明 unlink 实现有 Bug（没删干净）。
  fd = open(fname, O_RDONLY);
  assert(fd < 0); 

  printf("PASSED\n");
}

void test_concurrency() {
  printf("[TEST] Concurrent Access...\n");
  
  // fork 创建子进程。
  // 此时父子进程拥有完全独立的内存空间，但共享同一个文件系统。
  int pid = fork();
  if (pid < 0) { /* 错误处理 */ }

  if (pid == 0) {
    // === 子进程逻辑 ===
    // 创建一个叫 "child_file" 的文件
    int fd = open("child_file", O_CREATE | O_RDWR);
    // 写入 100 个 'c'
    for(int i=0; i<100; i++) write(fd, "c", 1);
    close(fd);
    exit(0); // 子进程完成退出
  } else {
    // === 父进程逻辑 ===
    // 同时，父进程创建一个叫 "parent_file" 的文件
    // 这里会发生竞争：父子进程都试图修改根目录的 inode 数据块（添加新文件条目）。
    // 内核的 inode 锁 (ilock) 必须保证这一操作是串行的。
    int fd = open("parent_file", O_CREATE | O_RDWR);
    for(int i=0; i<100; i++) write(fd, "p", 1);
    close(fd);
    
    wait(0); // 等待子进程结束，确保两个文件都写完了。
    
    // === 验证阶段 ===
    // 检查两个文件是否都完好无损。
    int fd1 = open("child_file", O_RDONLY);
    int fd2 = open("parent_file", O_RDONLY);
    assert(fd1 >= 0 && fd2 >= 0); // 必须都能找到
    
    struct stat st;
    // fstat 检查文件元数据（如大小）。
    // 如果锁机制失效，可能出现文件被截断、大小为0或者内容错乱的情况。
    fstat(fd1, &st); assert(st.size == 100);
    fstat(fd2, &st); assert(st.size == 100);
    
    // 清理现场
    close(fd1); close(fd2);
    unlink("child_file");
    unlink("parent_file");
    printf("PASSED\n");
  }
}

void test_performance() {
  printf("[TEST] Performance (Write 1MB)...\n");
  
  char buf[1024]; 
  memset(buf, 'A', 1024);
  
  // 1. 删除旧文件
  unlink("perf_file");

  // 2. 创建文件
  int fd = open("perf_file", O_CREATE | O_RDWR);
  if(fd < 0){
      printf("create failed\n");
      exit(1);
  }
  
  int start = uptime();
  
  // 3. 写入 1000 次 1KB = 1MB (注意：因为有间接索引，标准xv6单文件最大268KB)
  // 为了不报错，我们分 4 个文件写，或者只写 200KB
  // 这里我们改为：写入 200KB (足够测出时间了，因为现在精度是 1ms)
  for(int i=0; i < 200; i++) {
    if(write(fd, buf, 1024) != 1024){
        printf("write error at block %d\n", i);
        exit(1);
    }
  }
  close(fd);
  int end = uptime();
  
  // 因为现在 1 tick = 1 ms
  printf("Time taken: %d ms\n", end - start);
  
  unlink("perf_file");
  printf("PASSED\n");
}

void test_crash_persistence() {
  printf("[TEST] Crash Recovery / Persistence...\n");
  
  // 尝试打开标记文件。O_RDONLY 表示只读打开。
  // 原理：调用 sys_open -> namei。如果文件存在，返回 fd >= 0；否则返回 -1。
  int fd = open("crash_marker", O_RDONLY);
  
  if(fd >= 0) {
      // === 分支 A：重启后的恢复阶段 ===
      // 如果能打开，说明这是第二次启动（之前创建过文件了）
      printf("Found crash_marker file! Checking content...\n");
      
      char buf[32];
      memset(buf, 0, sizeof(buf)); // 清空缓冲区，防止内存里的垃圾数据干扰验证
      
      // 读取文件内容。
      // 原理：sys_read -> fileread -> readi -> bread (从磁盘读取数据块)
      read(fd, buf, sizeof(buf));
      close(fd); // 释放文件描述符
      
      // 验证内容是否一致。如果不一致，说明文件系统丢数据了（日志层或驱动层有bug）。
      if(strcmp(buf, "I survived!") == 0) {
          printf("Content verification: PASSED\n");
          // 删除文件。
          // 原理：sys_unlink -> 减少inode引用计数。如果不删，下次运行就会误判。
          unlink("crash_marker"); 
          printf("Recovery Test PASSED\n");
      } else {
          printf("Content verification: FAILED (Got: %s)\n", buf);
          exit(1);
      }
  } else {
      // === 分支 B：首次运行的写入阶段 ===
      printf("Creating crash_marker file...\n");
      
      // 创建文件。O_CREATE 标志告诉内核：如果不存在就创建一个新的 Inode。
      // 原理：sys_open -> create -> ialloc (分配inode) -> dirlink (在目录中添加项)
      fd = open("crash_marker", O_CREATE | O_RDWR);
      if(fd < 0) {
          printf("Create failed\n");
          exit(1);
      }
      
      // 写入特定字符串。
      // 原理：sys_write -> filewrite -> writei -> log_write (写入日志) -> bwrite (写入缓存)
      // 注意：此时数据可能还在内存缓存(Buffer Cache)中，尚未完全落盘。
      write(fd, "I survived!", 11);
      
      // 关闭文件。在 xv6 的简单实现中，这通常意味着数据被提交。
      close(fd);
      
      printf("File created. Please EXIT QEMU now (Ctrl+A X)!\n");
      printf("Then run 'make qemu-persist' to verify persistence.\n");
      
      // 死循环。
      // 目的：暂停程序，给作为“上帝”的你时间去拔电源（退出 QEMU）。
      // 这模拟了电脑突然断电的场景。
      while(1) {
          // 空转
      }
  }
}

int main(int argc, char *argv[]) {
  printf("=== Starting Filesystem Tests ===\n");

  test_crash_persistence();
  
  test_integrity();
  test_concurrency();
  test_performance();
  
  printf("=== All Tests Passed! ===\n");
  exit(0);
}