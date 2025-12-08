本分支包含 **从零构建操作系统** 课程实验5的完整实现代码。
在此阶段，内核已经从单线程裸机程序进化为一个**支持抢占式多任务（Preemptive Multitasking）** 的微型操作系统内核。

## 实验目标
- 实现进程控制块（PCB）和进程状态管理。
- 实现上下文切换（Context Switch）机制。
- 实现基于时间片的轮转调度器（Round-Robin Scheduler）。
- 实现时钟中断驱动的抢占式调度。
- 实现基础的进程同步机制（自旋锁、Sleep/Wakeup）。

## 完成功能
- **进程抽象**：定义了 `struct proc`，支持 `UNUSED`, `RUNNING`, `RUNNABLE`, `SLEEPING` 等状态。
- **上下文切换**：编写 `swtch.S` 汇编代码，实现了内核线程间的寄存器保存与恢复。
- **调度器**：实现了 `scheduler()` 核心循环，能够公平地调度多个内核线程。
- **抢占机制**：利用 RISC-V S 模式时钟中断，在 `kerneltrap()` 中触发 `yield()`，强制长任务让出 CPU。
- **同步原语**：实现了 `sleep()` 和 `wakeup()`，并通过生产者-消费者模型验证了其正确性。

## 关键文件说明

| 文件路径 | 说明 |
| :--- | :--- |
| `kernel/proc.h` | 定义进程结构体 `struct proc` 和上下文 `struct context` |
| `kernel/proc.c` | **核心实现**：进程创建(`allocproc`)、调度器(`scheduler`)、休眠唤醒(`sleep/wakeup`) |
| `kernel/swtch.S` | **核心汇编**：实现上下文切换的汇编代码 |
| `kernel/trap.c` | 修改了 `kerneltrap`，在时钟中断时检查并调用 `yield()` |
| `kernel/main.c` | 修改了启动流程，显式开启 S 模式中断分闸 (`sie`)，启动调度器 |
| `kernel/riscv.h` | 修正了 CSR 寄存器定义和中断相关的宏 |

## 编译与运行

确保已安装 RISC-V 工具链和 QEMU。

```bash
# 编译并启动 QEMU
make qemu

# 退出 QEMU
# 按 Ctrl+A，然后松开按 X
```

## 测试结果与解析

系统启动后会自动运行 `test_runner`，依次启动多个测试进程。以下是预期的正确输出日志及其含义：

```text
booting helloos...
kernel init done, starting processes...
Userinit: Created test_runner process (PID 1)

=== Starting Experiment 5 Tests ===
[Test] Process Creation...
SUCCESS: Created processes PID 2 and 3  <-- 进程创建/分配测试通过

=== Process Table ===
PID: 1 | State: RUNNING
PID: 2 | State: RUNNABLE
PID: 3 | State: RUNNABLE
=====================

... (测试任务启动) ...

Simple task running (PID 2)
Simple task running (PID 3)
Producer: Produced 10           <-- 生产者生产数据
Task PID 6 running iteration 0
...
Task PID 6 running iteration 3  <-- PID 6 正在执行长循环
Task PID 7 running iteration 0  <-- 【关键】PID 6 被强行打断，PID 7 插队运行（抢占成功）
Task PID 7 running iteration 1
...
Consumer: Consumed 10           <-- 【关键】消费者被唤醒并消费数据（同步成功）
Producer: Produced 20
Task PID 6 finished.            <-- PID 6 终于轮回来执行完毕
...
```

### 结果分析
1.  **轮转调度**：可以看到 PID 2, 3, 6, 7, 8 交替输出，证明调度器工作正常。
2.  **抢占验证**：PID 6 在打印 `iteration 3` 后并没有立即打印 `finished`，而是被 PID 7 和 Consumer 插入，证明时钟中断成功打断了正在运行的进程。
3.  **同步验证**：Consumer 严格在 Producer 生产后才进行消费，证明 `sleep` 和 `wakeup` 逻辑正确，且锁机制有效。

## 实现细节注意
- **中断开启**：为了解决裸机环境下 S 模式中断默认关闭的问题，我们在 `kernel/main.c` 中显式调用了 `w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);`。
- **内核栈**：每个进程在 `allocproc` 时通过 `kalloc` 分配了独立的内核栈，防止栈溢出或数据覆盖。
- **自旋锁**：在进程切换前后，严格遵守了锁的获取与释放规则，防止死锁。

---