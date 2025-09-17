.section .text
.global _entry
_entry:
    la sp, stack0
    li a0, 1024*4
    csrr a1, mhartid
    addi a1, a1, 1
    mul a0, a0, a1
    add sp, sp, a0
    /* --- 汇编清零 bss --- */
    la a0, __bss_start   /* 将符号地址载入 a0 */
    la a1, __bss_end     /* 将符号地址载入 a1 */
1:  beq a0, a1, 2f
    sb zero, 0(a0)
    addi a0, a0, 1
    j 1b
2:  /* 清零完成，跳到 C 的 start/main */
    call start
spin:
    j spin