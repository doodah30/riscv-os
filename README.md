这是 **从零构建操作系统 (riscv-os)** 的扩展实验项目。
在本实验中，我们将 xv6 原生的简单轮转调度器（Round-Robin）改造成了一个支持 **动态优先级** 的调度器。

## 实验目标与特性

1.  **优先级支持**：进程拥有 `priority` 属性 (0-10, 10最高)，调度器总是优先选择优先级最高的 `RUNNABLE` 进程。
2.  **同级公平性 (Round-Robin)**：当多个进程优先级相同时，调度器根据已运行时间 (`ticks`) 进行公平轮转，避免先来先服务 (FCFS)。
3.  **防饥饿老化 (Aging)**：引入老化机制，当低优先级进程等待时间过长时，动态提升其优先级。
4.  **系统调用**：新增 `setpriority` (设置优先级) 和 `ps` (查看进程状态) 系统调用。

## 测试指南：如何复现三种调度场景

为了验证调度器的不同特性，我们需要**修改内核代码**（开启/关闭 Aging）以及**切换用户测试程序**。

### 核心配置文件说明

1.  **内核配置** (`kernel/proc.c`): 控制是否开启老化 (Aging) 机制。
2.  **启动配置** (`user/init.c`): 控制启动时运行哪个测试程序 (`prio_test` 或 `same_prio_test`)。

---

### 场景 1：同级轮转 (Round-Robin)
**目标**：验证当优先级相同时，两个进程能否公平地交替运行。

1.  **修改内核 (`kernel/proc.c`)**：**关闭 Aging**
    *   找到 `update_process_times` 函数。
    *   **注释掉** 优先级提升的代码，防止干扰测试。
    ```c
    // kernel/proc.c -> update_process_times
    if(p->state == RUNNABLE) {
      p->wait_time++;
      /* 注释掉下面这段 Aging 逻辑
      if(p->wait_time > AGING_THRESHOLD) {
          if(p->priority < MAX_PRIO) p->priority++;
          p->wait_time = 0;
      }
      */
    }
    ```

2.  **修改启动项 (`user/init.c`)**：运行 `same_prio_test`
    ```c
    // user/init.c -> main
    char *argv[] = { "same_prio_test", 0 };
    exec("same_prio_test", argv);
    ```

3.  **运行**：
    ```bash
    make clean && make qemu
    ```
    **预期结果**：输出乱序（并发），Snapshot 中两个进程 `TICKS` 数量几乎相等 (e.g., 443 vs 443)。

---

### 场景 2：严格优先级 (Strict Priority)
**目标**：验证高优先级进程是否能完全压制低优先级进程（低优先级饿死）。

1.  **修改内核 (`kernel/proc.c`)**：**关闭 Aging** (同场景 1)
    *   保持 Aging 代码被注释的状态。

2.  **修改启动项 (`user/init.c`)**：运行 `prio_test`
    ```c
    // user/init.c -> main
    char *argv[] = { "prio_test", 0 };
    exec("prio_test", argv);
    ```

3.  **运行**：
    ```bash
    make clean && make qemu
    ```
    **预期结果**：High Prio 先打印 finished。Snapshot 中 High Prio 已经跑完 (ZOMBIE)，Low Prio 还没开始跑 (TICKS=0)。

---

### 场景 3：老化机制 (Aging)
**目标**：验证低优先级进程在等待足够长的时间后，能否被提升优先级并获得 CPU。

1.  **修改内核 (`kernel/proc.c`)**：**开启 Aging**
    *   找到 `update_process_times` 函数。
    *   **解除注释**，恢复 Aging 逻辑。
    ```c
    // kernel/proc.c -> update_process_times
    if(p->state == RUNNABLE) {
      p->wait_time++;
      // === 解除注释 ===
      if(p->wait_time > AGING_THRESHOLD) {
          if(p->priority < MAX_PRIO) p->priority++;
          p->wait_time = 0;
      }
      // ===============
    }
    ```

2.  **修改启动项 (`user/init.c`)**：运行 `prio_test` (同场景 2)
    *   保持运行 `prio_test`。

3.  **运行**：
    ```bash
    make clean && make qemu
    ```
    **预期结果**：Low Prio 不再为 0 TICKS。Snapshot 中可以看到 Low Prio 的优先级从初始值 (2) 被提升到了更高 (如 9 或 10)。Low Prio 最终能完成运行。

---

## 关键文件说明

| 文件 | 说明 |
| :--- | :--- |
| `kernel/proc.h` | 增加了 `priority`, `ticks`, `wait_time` 字段 |
| `kernel/proc.c` | **`scheduler`**: 实现了寻找最高优先级 + 同级 tick 均衡的逻辑<br>**`update_process_times`**: 实现了 Aging 逻辑 |
| `kernel/sysproc.c` | 实现了 `sys_setpriority` 和 `sys_ps` |
| `user/prio_test.c` | 测试高低优先级进程的竞争 (用于场景 2 和 3) |
| `user/same_prio_test.c` | 测试同级优先级进程的轮转 (用于场景 1) |
| `user/ps.c` | 用户态进程查看工具 |

## 调度算法伪代码

```c
// 1. 遍历所有 RUNNABLE 进程
for p in proc:
    // 策略 A: 优先级更高者优先
    if p.priority > max_prio:
        best = p
    
    // 策略 B: 优先级相同时，运行时间(ticks)少者优先 (Round-Robin)
    else if p.priority == max_prio:
        if p.ticks < best.ticks:
            best = p
```

---