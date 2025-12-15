# user/usys.S
# 生成系统调用汇编代码的宏

.macro syscall name
.global \name
\name:
    li a7, SYS_\name
    ecall
    ret
.endm

# 引入内核的系统调用号定义
# 注意：你需要确保编译时能找到 kernel/syscall.h
# 这里为了简单，我们手动硬编码几个常用的，或者让编译器去 include

#include "kernel/syscall.h"

syscall exec
syscall exit
syscall write