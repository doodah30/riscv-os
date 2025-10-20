#ifndef DEFS_H
#define DEFS_H

#include "types.h"
// bio.c


// console.c
void            consoleinit(void);
void            clear(void);
void            consputc(int);
void            goto_xy(int x, int y);
void            clear_line(void);
// exec.c

// file.c

// fs.c

// kalloc.c

// log.c

// pipe.c

// printf.c
void            printf(const char *fmt, ...);
void            printf_color(int color, const char *fmt, ...);
void            panic(char *s);
// proc.c

// swtch.S

// spinlock.c

// sleeplock.c

// string.c

// syscall.c

// trap.c
void            trapinithart(void);
// uart.c
void            uartinit(void);
void            uartputs(const char *s);
void            uartputc(int);
void            uartintr(void);

// vm.c

// plic.c
void            plicinit(void);
void            plicinithart(void);
int             plic_claim(void);
void            plic_complete(int);
// virtio_disk.c
void            virtio_disk_intr(void);

#endif // DEFS_H
