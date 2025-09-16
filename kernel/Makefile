# RISC-V 交叉工具链前缀
CROSS_COMPILE = riscv64-unknown-elf-
CC      = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy

# 编译选项：使用 medany（允许放到 0x80000000 这类地址）
CFLAGS = -march=rv64gc -mabi=lp64 -mcmodel=medany -O2 -Wall -ffreestanding -nostdlib
# 链接选项：使用 medany，并且在链接时也不要链接标准库
LDFLAGS = -T kernel.ld -mcmodel=medany -nostdlib

OBJS = entry.o start.o uart.o

all: kernel.elf

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 用 gcc 驱动链接，但同时明确 -nostdlib（放在 LDFLAGS 中）
kernel.elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

qemu: kernel.elf
	qemu-system-riscv64 -machine virt -nographic -bios none -kernel kernel.elf

clean:
	rm -f *.o kernel.elf kernel.bin
