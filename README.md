# 从零构建 RISC-V 操作系统

欢迎来到 **riscv-os** 项目！

这是一个用于教学和学习目的的操作系统内核实现项目。本项目遵循循序渐进的原则，从第一行汇编代码开始，逐步构建一个运行在 RISC-V 架构（QEMU 模拟器）上的类 Unix 操作系统。

本系统最终实现了一个具备**虚拟内存**、**抢占式调度**、**文件系统**、**系统调用**以及**用户 Shell** 的完整微内核。

## 项目结构与分支导航

> **重要提示**：
> 本仓库采用 **分支 (Branch)** 管理开发进度。
> `main` 分支仅作为项目索引。**请切换到对应的实验分支**以查看具体阶段的完整代码、实现细节和该阶段的 `README` 文档。

请根据下表切换到相应的分支查看：

| 实验阶段 | 实验主题 | 核心功能与技术点 | 状态 | 
| :--- | :--- | :--- | :--- |
| **Lab 0** | 开发环境搭建 | 工具链安装 (GCC, QEMU), GDB 调试配置 | 完成 |
| **Lab 1** | RISC-V 引导与裸机启动 | `entry.S` 汇编启动, 栈设置, UART 串口驱动, Hello OS | 完成 | first |
| **Lab 2** | 内核输出与库函数 | 格式化输出 `printf` 实现, 显存控制, 字符串处理 | 完成 | second |
| **Lab 3** | 页表与内存管理 | Sv39 虚拟内存, 页表映射, 物理内存分配器 (`kalloc`) | 完成 | third |
| **Lab 4** | 中断处理与时钟 | Trap 机制, 上下文保存, 时钟中断 (`timer`), PLIC 初始化 | 完成 | fourth |
| **Lab 5** | 进程管理与调度 | 进程控制块 (PCB), 上下文切换 (`swtch`), 抢占式轮转调度 | 完成 | fifth |
| **Lab 6** | 系统调用 | 用户态/内核态切换 (`trampoline`), `ecall` 处理, 基础 Syscalls | 完成 | sixth |
| **Lab 7** | 文件系统 | VirtIO 驱动, Buffer Cache, 日志系统, Inode, `exec` 加载器 | 完成 | seventh |
| **Lab 8** | 系统扩展 | 优先级调度器, 防饥饿 Aging 机制 | 完成 | eighth |

## 如何开始 (Getting Started)

### 1. 克隆仓库
```bash
git clone https://github.com/doodah30/riscv-os.git
cd riscv-os
```

### 2. 切换到具体实验
例如，如果你想查看 **文件系统 (Lab 7)** 的代码：
```bash
# 查看所有远程分支
git branch -r

# 切换到实验7的分支
git checkout seventh
```

### 3. 编译与运行
确保你已经安装了 `qemu-system-riscv64` 和 `riscv64-unknown-elf-gcc`。
进入对应分支后：(请以分支内README为参考)

```bash
# 编译并启动 QEMU
make qemu

# 退出 QEMU
# 按下 Ctrl+A，松开后按 X
```

## 最终成果展示 (基于 Lab 8)

在最终的实验分支中，操作系统已经具备了以下高级特性：

*   **Shell 交互**：支持基础的用户态 Shell 和命令执行。
*   **持久化存储**：支持 `Crash-safe` 的日志文件系统，断电数据不丢失。
*   **多道程序**：支持 `fork`, `exec`, `wait`, `exit` 等进程生命周期管理。
*   **智能调度**：实现了支持优先级、同级公平轮转（Round-Robin）和防饥饿老化（Aging）的调度算法。
*   **用户程序**：能够运行 C 语言编写的 `init`, `fstest` (文件系统测试), `prio_test` (调度测试) 等程序。

## 文档说明

每个实验分支的根目录下都有一个独立的 `README.md`，其中包含了：
*   该阶段的具体实现细节。
*   遇到的核心 Bug 及解决方案（Troubleshooting）。
*   测试方法与运行截图。

请前往对应分支阅读详细文档。

---

*Created by [doodah30]*
