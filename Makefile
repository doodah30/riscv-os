# 交叉编译工具链前缀
CROSS_COMPILE = riscv64-unknown-elf-
CC      = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy

# 编译选项：使用 medany（允许放到 0x80000000 这类地址）
CFLAGS  = -march=rv64gc -mabi=lp64 -mcmodel=medany -O0 -Wall -ffreestanding -nostdlib -g -fno-omit-frame-pointer
# 链接选项：使用 medany，并且在链接时也不要链接标准库
LDFLAGS = -T kernel/kernel.ld -mcmodel=medany -nostdlib

# 内核目录
K = kernel

# --------------------------------------------------
OBJS := \
    $(K)/entry.o  \
    $(K)/main.o   \
    $(K)/uart.o   \
	$(K)/console.o\
	$(K)/printf.o \
	$(K)/kalloc.o \
	$(K)/vm.o     \
	$(K)/string.o \
	$(K)/start.o  \
  	$(K)/plic.o   \
  	$(K)/trap.o   \
	$(K)/virtio_disk.o\
	$(K)/kernelvec.o

OBJS_ALL = $(OBJS)        # 手动列清单

# --------------------------------------------------

all: kernel.elf

# 规则：汇编文件
$(K)/%.o: $(K)/%.S
	@$(CC) $(CFLAGS) -c $< -o $@

# 规则：C 文件
$(K)/%.o: $(K)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@

# 链接
kernel.elf: $(OBJS_ALL)
	@$(CC) $(LDFLAGS) -o $@ $(OBJS_ALL)

# 生成二进制镜像
kernel.bin: kernel.elf
	@$(OBJCOPY) -O binary kernel.elf kernel.bin

# QEMU 运行
qemu: kernel.elf
	@qemu-system-riscv64 -machine virt -nographic -bios none -kernel kernel.elf

# 用于 GDB 调试的规则
# -S: 启动后冻结CPU，等待GDB连接
# -s: 在 1234 端口开启GDB服务 (是 -gdb tcp::1234 的简写)
qemu-gdb: kernel.elf
	@qemu-system-riscv64 -machine virt -nographic -bios none -kernel kernel.elf -S -s

# 清理
clean:
	@rm -f $(K)/*.o kernel.elf kernel.bin
