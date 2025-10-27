# kernel/kernelvec.S

.section .text
.globl kernelvec
.align 4
kernelvec:
        # --- 保存现场 ---
        # 在栈上开辟 256 字节的空间 (32 个寄存器 * 8 字节/寄存器)。
        addi sp, sp, -256
        # 依次将 ra (返回地址), sp (栈指针), gp (全局指针), tp (线程指针),
        # 所有临时寄存器 (t0-t2), 所有保存寄存器 (s0-s11),
        # 所有参数/返回值寄存器 (a0-a7) 的值存入栈中。
        # 这是至关重要的一步，确保了中断处理完毕后，
        # 被中断的程序可以恢复到完全相同的状态。
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
        # --- 调用 C 代码 ---
        # 所有状态都已安全保存，现在可以安全地调用 C 语言函数了。
        call kerneltrap
        # --- 恢复现场 ---
        # 从栈中，按照与保存时相反的顺序，将所有寄存器的值恢复回来。
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
        # 恢复栈指针到进入中断前的位置。
        addi sp, sp, 256
	# --- 从中断返回 ---
    # 执行 sret (Supervisor Return from Trap) 指令。
    # CPU 会将 sepc 的值加载回 PC，并恢复 sstatus 寄存器的状态，
    # 从而返回到被中断的程序继续执行。
	sret