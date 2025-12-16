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

# 指定用户目录
U=user

# 2. 编译用户程序的规则
$U/init.o: $U/init.c
	$(CC) $(CFLAGS) -c $U/init.c -o $U/init.o

$U/usys.o: $U/usys.S
	$(CC) $(CFLAGS) -c $U/usys.S -o $U/usys.o

# 链接成可执行文件 _init
# 注意：我们需要一个简单的链接脚本或者入口点。
# xv6 使用 _main 作为入口，这里简化直接链接。
ULDFLAGS = -march=rv64gc -mabi=lp64 -mcmodel=medany -nostdlib

$U/ulib.o: $U/ulib.c
	$(CC) $(CFLAGS) -c $U/ulib.c -o $U/ulib.o

# 2. 修改 _init 的链接规则 (加上 $U/ulib.o)
$U/_init: $U/init.o $U/usys.o $U/ulib.o
	$(CC) $(ULDFLAGS) -N -e main -Ttext 0 -o $U/_init $U/init.o $U/usys.o $U/ulib.o
	$(OBJCOPY) -S $U/_init $U/init.asm

# 3. 修改 _fstest 的链接规则 (加上 $U/ulib.o)
$U/_fstest: $U/fstest.o $U/usys.o $U/ulib.o
	$(CC) $(ULDFLAGS) -N -e main -Ttext 0 -o $U/_fstest $U/fstest.o $U/usys.o $U/ulib.o
	$(OBJCOPY) -S $U/_fstest $U/fstest.asm

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