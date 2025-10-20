# kernel/kernelvec.S

.section .text
.align 4
.globl timervec
timervec:
    # 使用 mscratch 原子地交换 a0，并保存 a1, t0, t1
    csrrw a0, mscratch, a0
    # 读取 mstatus
    csrr t0, mstatus
    # 清除 mstatus 中的 MIE 位 (M-mode Interrupt Enable)，即关闭M-mode中断
    li t1, 8       # MSTATUS_MIE = 1 << 3 = 8
    not t1, t1
    and t0, t0, t1
    # 写回 mstatus
    csrw mstatus, t0
    sd a1, 0(a0)

    # 从 scratch 区域加载 CLINT MTIMECMP 寄存器的地址和中断间隔
    ld t0, 24(a0) # scratch[3] = CLINT_MTIMECMP(hart)
    ld t1, 32(a0) # scratch[4] = interval

    # 读取当前的 MTIMECMP 值
    ld a1, 0(t0)
    # 计算下一次中断的时间点
    add a1, a1, t1
    # 写回 MTIMECMP 寄存器，预约下一次时钟中断
    sd a1, 0(t0)

    # 向 Supervisor-mode 发送一个软件中断 (SSIP bit = 1)
    li a1, 2
    csrw sip, a1

    # 恢复寄存器
    ld a1, 0(a0)
    csrr t0, mstatus
    # 置位 mstatus 中的 MIE 位，即重新开启M-mode中断
    li t1, 8
    or t0, t0, t1
    # 写回 mstatus
    csrw mstatus, t0
    # 换回 a0
    csrrw a0, mscratch, a0
    
    # 从 M-mode 中断返回。这将返回到 .bss 循环中被中断的那一点
    mret

#
# supervisor-mode trap handler
#
.globl kernelvec
.align 4
kernelvec:
        addi sp, sp, -256
        sd ra, 0(sp)
        sd sp, 8(sp)
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd s0, 56(sp)
        sd s1, 64(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd s2, 136(sp)
        sd s3, 144(sp)
        sd s4, 152(sp)
        sd s5, 160(sp)
        sd s6, 168(sp)
        sd s7, 176(sp)
        sd s8, 184(sp)
        sd s9, 192(sp)
        sd s10, 200(sp)
        sd s11, 208(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd x31, 240(sp)  # 使用硬件名称 x31 替代 t6

        call kerneltrap

        ld ra, 0(sp)
        ld sp, 8(sp)
        ld gp, 16(sp)
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld s0, 56(sp)
        ld s1, 64(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld s2, 136(sp)
        ld s3, 144(sp)
        ld s4, 152(sp)
        ld s5, 160(sp)
        ld s6, 168(sp)
        ld s7, 176(sp)
        ld s8, 184(sp)
        ld s9, 192(sp)
        ld s10, 200(sp)
        ld s11, 208(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld x31, 240(sp)  # 使用硬件名称 x31 替代 t6
        addi sp, sp, 256
	
	sret