这是 **从零构建操作系统 (riscv-os)** 的第七个实验阶段。
在本实验中，我们将操作系统从一个单纯的内存驻留程序，升级为一个**能够持久化存储数据、加载并运行磁盘程序**的完整内核。

我们实现了一个类 xv6 的文件系统栈，支持崩溃一致性（Crash Safety），并打通了从磁盘加载 ELF 二进制文件 (`exec`) 的全过程。

## 实验目标
1.  **底层驱动**：实现 VirtIO-Blk 磁盘驱动，通过 MMIO 与 QEMU 模拟的磁盘设备交互。
2.  **文件系统核心**：实现缓冲区缓存 (`bio.c`)、日志系统 (`log.c`)、Inode 管理 (`fs.c`) 和文件描述符抽象 (`file.c`)。
3.  **系统调用扩展**：实现文件操作相关的系统调用 (`open`, `write`, `read`, `mkdir`, `dup`, `fstat` 等)。
4.  **用户程序加载**：实现 `exec` 系统调用，解析 ELF 文件头，将用户程序加载到内存并执行。
5.  **用户态环境**：编写用户态库 (`ulib.c`) 和测试程序 (`init.c`, `fstest.c`)。

## 完成功能特性

### 1. 存储系统
- **VirtIO 驱动**：支持 VirtIO MMIO 协议（强制 Version 2），实现块设备的读写。
- **Buffer Cache**：基于 LRU 策略和睡眠锁（Sleep Lock）的双向链表缓存机制。
- **日志系统 (Logging)**：实现预写日志（Write-Ahead Log），保证文件系统元数据操作的原子性。
- **文件抽象**：支持 Inode、目录、路径名解析，以及标准输入输出（Console）设备文件。

### 2. 进程与内存
- **Exec 加载器**：废弃了手写的 `initcode` 机器码，改为从磁盘加载标准的 ELF 可执行文件。
- **内存管理增强**：支持用户栈的 Guard Page（保护页），支持稀疏内存布局的释放。
- **完善的销毁逻辑**：实现了 `proc_freepagetable`，解决了进程退出时 Trampoline 和 Trapframe 的映射清理问题。

### 3. 用户态工具
- **`init` 进程**：系统的第一个用户进程，负责初始化控制台并启动测试。
- **`fstest`**：综合测试套件，涵盖完整性、并发、持久化和性能测试。
- **`printf`**：实现了支持 64 位参数解析的用户态格式化输出。

## 关键技术挑战与解决方案

本实验经历了深度的调试过程，解决了以下核心难题：

1.  **VirtIO 协议版本不匹配**
    - **现象**：驱动初始化时读取 Magic Number 成功，但发送请求后死锁，不产生中断。
    - **原因**：驱动代码基于 VirtIO v2 (Modern)，而 QEMU 默认提供 v1 (Legacy)。
    - **解决**：在 Makefile 中添加 `-global virtio-mmio.force-legacy=false` 强制 QEMU 使用 Modern 协议。

2.  **启动阶段的死锁 (Panic: holding lk is NULL)**
    - **现象**：`main` 函数初始化文件系统时触发 Panic。
    - **原因**：`fsinit` 调用磁盘读写时，当前 CPU 尚未运行任何进程 (`myproc() == NULL`)，但 `sleep` 函数试图获取当前进程的锁。
    - **解决**：调整初始化顺序（先中断后 FS），并修改 `sleep` 函数，在无进程上下文时回退为自旋等待中断 (`wfi`)。

3.  **中断丢失与委托**
    - **现象**：磁盘操作完成后，内核无法收到 PLIC 中断，导致驱动无限休眠。
    - **原因**：M-mode 启动代码 (`start.c`) 未设置 `mideleg`，导致外部中断被 M-mode 拦截。
    - **解决**：设置 `w_mideleg(0xffff)` 将所有中断委托给 S-mode 处理。

4.  **Exec 内存释放崩溃 (Panic: freewalk leaf)**
    - **现象**：`exec` 销毁旧页表时 Panic。
    - **原因**：`uvmunmap` (或 `uvmclear`) 仅清除了用户权限位，未清除有效位 (`PTE_V`)，导致 `freewalk` 误判页表非空。
    - **解决**：强制在解映射时将 PTE 清零，并在 `vm.c` 中正确处理 Guard Page 的跳过逻辑。

## 编译与运行

### 1. 编译并启动
```bash
# 编译内核、用户程序、制作文件系统镜像并启动 QEMU
make qemu
```

### 2. 持久化测试 (模拟断电)
测试文件系统是否真的将数据写入了磁盘：

1.  运行 `make qemu`。
2.  看到 `Please EXIT QEMU now!` 提示时，按下 `Ctrl+A` 松开后按 `X` 强制退出。
3.  运行以下命令（保留旧的 `fs.img`）：
    ```bash
    make qemu-persist
    ```
4.  系统应报告 `Recovery Test PASSED`。

## 测试结果

系统启动后会自动运行 `fstest`，预期输出如下：

```text
booting helloos...
init: starting fstest...
=== Starting Filesystem Tests ===
[TEST] Filesystem Integrity...
PASSED
[TEST] Concurrent Access...
PASSED
[TEST] Performance (Write 1MB)...
Time taken: 2662 ms(或者一些近似的值)
PASSED
=== All Tests Passed! ===
init: test finished.
```

## 📂 目录结构说明

- `kernel/fs.c`: 文件系统核心（Inode, Directory, Path）。
- `kernel/bio.c`: 缓冲区缓存 (Buffer Cache)。
- `kernel/log.c`: 日志层 (Transactions)。
- `kernel/virtio_disk.c`: 磁盘驱动。
- `kernel/sysfile.c`: 文件相关系统调用接口。
- `kernel/exec.c`: ELF 文件加载器。
- `user/`: 用户态程序 (`init.c`, `fstest.c`, `ulib.c`)。
- `mkfs/`: 宿主机工具，用于制作 `fs.img`。
