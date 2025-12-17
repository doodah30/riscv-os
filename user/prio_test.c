#include "kernel/types.h"
#include "user/user.h"

void spin(int limit) {
    int i, j;
    for(i=0; i<limit; i++) {
        for(j=0; j<1000000; j++) {
            // 空转，消耗 CPU
            asm volatile("nop"); 
        }
    }
}

int main() {
    int pid1, pid2;
    int start_time;

    printf("Starting Priority Scheduler Test...\n");

    // 创建低优先级进程 (Child 1)
    pid1 = fork();
    if(pid1 == 0) {
        setpriority(getpid(), 2); // 优先级 2 (低)
        start_time = uptime();
        spin(500);
        printf("Low Prio (PID %d) finished. Duration: %d\n", getpid(), uptime() - start_time);
        exit(0);
    }

    // 创建高优先级进程 (Child 2)
    pid2 = fork();
    if(pid2 == 0) {
        setpriority(getpid(), 8); // 优先级 8 (高)
        start_time = uptime();
        spin(500);
        printf("High Prio (PID %d) finished. Duration: %d\n", getpid(), uptime() - start_time);
        exit(0);
    }

    printf("Parent sleeping to let children run...\n");
    sleep(200); // 睡 200 个 tick (大约 2秒)

    printf("\n--- Snapshot of Running Processes ---\n");
    ps(); // 这时候子进程肯定还在跑，能看到它们的状态
    printf("-------------------------------------\n");

    // 父进程等待
    wait(0);
    wait(0);
    
    exit(0);
}