#include "kernel/types.h"
#include "user/user.h"

void spin(int limit) {
    int i, j;
    for(i=0; i<limit; i++) {
        for(j=0; j<1000000; j++) {
            asm volatile("nop"); 
        }
    }
}

int main() {
    int pid1, pid2;
    int start_time;

    printf("Starting Same Priority (RR) Test...\n");

    // 1. 创建子进程 A (默认优先级 5)
    pid1 = fork();
    if(pid1 == 0) {
        // 子进程先睡一小会儿，等待父进程把自己提拔到 10
        sleep(5); 
        start_time = uptime();
        spin(200); // 减少一点循环量，方便观察
        printf("Child A (PID %d) finished. Duration: %d\n", getpid(), uptime() - start_time);
        exit(0);
    }

    // 2. 创建子进程 B (默认优先级 5)
    pid2 = fork();
    if(pid2 == 0) {
        sleep(5);
        start_time = uptime();
        spin(200);
        printf("Child B (PID %d) finished. Duration: %d\n", getpid(), uptime() - start_time);
        exit(0);
    }

    // 3. === 关键步骤 ===
    // 此时父进程还在运行 (Prio 5)，子进程也在 (Prio 5)
    // 我们现在把两个子进程同时提拔到最高优先级 (10)
    printf("Parent elevating children to Prio 10...\n");
    setpriority(pid1, 10);
    setpriority(pid2, 10);

    // 4. 父进程睡觉，让出 CPU 给两个高优先级的子进程
    printf("Parent sleeping to observe RR behavior...\n");
    sleep(100); 

    // 5. 醒来拍快照
    // 此时 A 和 B 应该都在运行，且 TICKS 应该差不多
    printf("\n--- Snapshot ---\n");
    ps(); 
    printf("----------------\n");

    // 6. 回收
    wait(0);
    wait(0);
    
    printf("Test finished.\n");
    exit(0);
}