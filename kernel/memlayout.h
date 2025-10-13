#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H
#define KERNBASE 0x80000000UL
#define PHYSTOP  (KERNBASE + 128*1024*1024) // example 128MB, 调整为你的平台
#define UART0    0x10000000UL
/* TRAMPOLINE 如果未实现可暂时注释或定义为 0 */
#endif