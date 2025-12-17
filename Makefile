# 交叉编译工具链前缀
CROSS_COMPILE = riscv64-unknown-elf-
CC      = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy

# 编译选项：使用 medany（允许放到 0x80000000 这类地址）
CFLAGS  = -march=rv64gc -mabi=lp64 -mcmodel=medany -O0 -Wall -ffreestanding -nostdlib -g -fno-omit-frame-pointer -I.
# 链接选项：使用 medany，并且在链接时也不要链接标准库
LDFLAGS = -T kernel/kernel.ld -mcmodel=medany -nostdlib

# 内核目录
K = kernel

# --------------------------------------------------
OBJS := \
    $(K)/entry.o  \
    $(K)/start.o  \
    $(K)/main.o   \
    $(K)/uart.o   \
    $(K)/console.o\
    $(K)/printf.o \
    $(K)/kalloc.o \
    $(K)/vm.o     \
    $(K)/string.o \
    $(K)/plic.o   \
    $(K)/trap.o   \
    $(K)/kernelvec.o \
    $(K)/proc.o   \
    $(K)/swtch.o  \
    $(K)/spinlock.o \
    $(K)/sleeplock.o \
    $(K)/trampoline.o \
    $(K)/syscall.o \
    $(K)/sysproc.o \
    $(K)/virtio_disk.o \
    $(K)/bio.o   \
    $(K)/fs.o \
    $(K)/log.o \
    $(K)/file.o \
    $(K)/sysfile.o \
	$(K)/pipe.o \
    $(K)/exec.o

OBJS_ALL = $(OBJS)        # 手动列清单

# --------------------------------------------------

# 1. 定义用户程序列表
UPROGS=\
    $U/_init\
    $U/_fstest\
    $U/_systest\
    $U/_prio_test\
    $U/_ps\
    $U/_nice\
    $U/_same_prio_test\

# 指定用户目录
U=user

# 用户链接选项 (不使用内核 ld 脚本，代码段从 0 开始)
ULDFLAGS = -march=rv64gc -mabi=lp64 -mcmodel=medany -nostdlib

# 2. 编译汇编库 (usys.S -> usys.o)
$U/usys.o: $U/usys.S
	$(CC) $(CFLAGS) -c $U/usys.S -o $U/usys.o

# 3. [新增/关键] 通用 C 文件编译规则 (.c -> .o)
# 这条规则适用于 init.c, fstest.c, prio_test.c, ulib.c 等所有用户 C 文件
$U/%.o: $U/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$U/_%: $U/%.o $U/usys.o $U/ulib.o
	$(CC) $(ULDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJCOPY) -S $@ $U/$*.asm

all: kernel.elf fs.img

# 规则：汇编文件
$(K)/%.o: $(K)/%.S
	@$(CC) $(CFLAGS) -c $< -o $@

# 规则：C 文件
$(K)/%.o: $(K)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@

# 链接
kernel.elf: $(OBJS_ALL) kernel/kernel.ld
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_ALL)

# 生成二进制镜像
kernel.bin: kernel.elf
	@$(OBJCOPY) -O binary kernel.elf kernel.bin

mkfs/mkfs: mkfs/mkfs.c
	gcc -Werror -Wall -I. -o mkfs/mkfs mkfs/mkfs.c

# 生成 fs.img
# 这里暂时创建一个空的 README 文件进去演示，以后你可以放用户程序
fs.img: mkfs/mkfs $(UPROGS)
	./mkfs/mkfs fs.img $(UPROGS)

# QEMU 基本参数
QEMUOPTS = -machine virt -nographic -bios none -kernel kernel.elf -m 128M -smp 1

# 挂载磁盘参数 (VirtIO)
# drive: 定义一个驱动器，文件是 fs.img
# device: 定义一个设备，连接到该驱动器
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
QEMUOPTS += -global virtio-mmio.force-legacy=false

# QEMU 运行
qemu: kernel.elf fs.img
	@qemu-system-riscv64 $(QEMUOPTS)

# 用于 GDB 调试的规则
# -S: 启动后冻结CPU，等待GDB连接
# -s: 在 1234 端口开启GDB服务 (是 -gdb tcp::1234 的简写)
qemu-gdb: kernel.elf  fs.img
	@qemu-system-riscv64 $(QEMUOPTS) -S -s

# 清理
clean:
	@rm -f $(K)/*.o kernel.elf kernel.bin fs.img mkfs/mkfs
	@rm -f user/*.o user/*.asm user/_*

qemu-persist: kernel.elf
	@qemu-system-riscv64 $(QEMUOPTS)