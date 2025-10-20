# kernel/entry.S (Final, Robust Version)
.section .text
.global _entry
_entry:

    la sp, stack0
    li t1, 4096
    csrr t0, mhartid
    add t0, t0, 1
    mul t0, t0, t1
    add sp, sp, t0

    # 3. 清零 .bss 段 (这一步现在是安全的)
    la a0, __bss_start
    la a1, __bss_end
1:  beq a0, a1, 2f
    sb zero, 0(a0)
    addi a0, a0, 1
    j 1b
2:
    # 4. 跳转到 M-mode 的 C 入口函数 start
    call start

spin:
    j spin