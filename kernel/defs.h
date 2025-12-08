#ifndef DEFS_H
#define DEFS_H

#include "types.h"

struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct spinlock;  // <--- 重点：必须添加这一行！
struct stat;
struct superblock;

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
void kinit(void *start, void *endpa); // start/end are kernel-accessible addresses (VA)
void *kalloc(void);                    // return a kernel-accessible page (VA) or NULL
void kfree(void *pa);
// log.c

// pipe.c

// printf.c
void            printf(const char *fmt, ...);
void            printf_color(int color, const char *fmt, ...);
void            panic(char *s);
// proc.c
int             cpuid(void);
void            exit(int);
int             fork(void);
int             growproc(int);
void            proc_mapstacks(pagetable_t);
pagetable_t     proc_pagetable(struct proc *);
void            procinit(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             wait(uint64);
void            wakeup(void*);
void            yield(void);
int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void            procdump(void);
struct proc*    myproc(void);
struct cpu*     mycpu(void);
// swtch.S
void            swtch(struct context*, struct context*);
// spinlock.c
void            initlock(struct spinlock*, char*);
void            acquire(struct spinlock*);
void            release(struct spinlock*);
int             holding(struct spinlock*);
void            push_off(void);
void            pop_off(void);
// sleeplock.c

// string.c
int             memcmp(const void*, const void*, uint);
void*           memmove(void*, const void*, uint);
void*           memset(void*, int, uint);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, uint);
char*           strncpy(char*, const char*, int);
void*           memcpy(void *dest, const void *src, size_t n);
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
