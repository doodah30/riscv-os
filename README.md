## **项目状态**

当前，本分支已经完成了一个重要的里程碑：**成功实现了中断处理框架与时钟管理**。

这意味着内核不再是一个只能顺序执行指令的静态程序，而是变成了一个能够响应外部异步事件的动态系统。这是实现所有现代操作系统并发功能（如多任务调度）的基石。

### **已实现的核心功能**

*   **基础启动流程**: 内核能够从 QEMU 模拟的 `virt` 机器上正确启动，完成从 M-mode 到 S-mode 的权限切换。
*   **内存管理**: 实现了基于页表的虚拟内存管理，包括内核空间的映射和物理内存的分配（`kalloc`/`kfree`）。
*   **控制台 I/O**: 实现了基于 UART 的 `printf` 函数，提供了内核与用户交互的基础通道。
*   **中断处理**:
    *   构建了符合 RISC-V 架构的 S-mode 统一 trap 处理机制。
    *   实现了对 **S-mode 时钟中断 (Supervisor Timer Interrupt)** 的正确响应和处理。
    *   集成了对 **PLIC (Platform-Level Interrupt Controller)** 的驱动，为未来响应外部设备（如键盘、磁盘）中断做好了准备。
*   **时钟管理**:
    *   通过配置 RISC-V 的硬件定时器，实现了周期性的时钟中断。
    *   在控制台周期性打印 "tick" 信息，直观地展示了时钟中断正在正常工作。

## **如何构建与运行**

### **环境要求**

1.  **RISC-V 交叉编译工具链**: 你需要一个标准的 `riscv64-unknown-elf-` 工具链（包含 GCC, Binutils, GDB 等）。
2.  **QEMU**: 需要支持 `riscv64` 架构的 QEMU (`qemu-system-riscv64`)。

### **编译**

在项目根目录下，直接运行 `make` 即可。

```bash
make
```

这将编译所有内核源代码，并链接生成一个名为 `kernel.elf` 的内核镜像。

### **运行**

使用 `make qemu` 命令来启动 QEMU 并运行内核。

```bash
make qemu
```

如果一切正常，你将看到类似以下的输出，并且 "tick" 会周期性地出现：

```
booting helloos...
setup complete; waiting for interrupts.
tick
tick
tick
...
```

要退出 QEMU，通常可以按下 `Ctrl+A` 然后按 `X`。

### **调试**

本项目支持使用 GDB进行源码级调试。

1.  **在第一个终端**，启动 QEMU 并使其等待 GDB 连接：
    ```bash
    make qemu-gdb
    ```

2.  **在第二个终端**，启动 GDB 并连接：
    ```bash
    riscv64-unknown-elf-gdb
    ```
    在 GDB 内部，输入以下命令来加载符号文件并连接到 QEMU：
    ```gdb
    (gdb) file kernel.elf
    (gdb) target remote localhost:1234
    ```
    现在，你可以使用标准的 GDB 命令（如 `b`, `c`, `n`, `si`, `p`）来调试内核了。

## **下一步计划**

在实现了中断和时钟的基础上，接下来的核心目标是：

*   **进程管理**: 实现 `proc` 结构体，创建第一个用户进程。
*   **抢占式调度**: 利用时钟中断，实现一个简单的轮转调度器 (Round-Robin Scheduler)。
*   **系统调用**: 构建完整的系统调用接口，让用户进程能够与内核交互。

---
