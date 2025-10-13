// kalloc.c - simple page frame allocator (freelist in-page)
#include "kmem.h"
#include "riscv.h"
#include "defs.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef KMEM_USE_LOCK
#define KMEM_USE_LOCK 1
#endif

#if KMEM_USE_LOCK
// You should provide simple spinlock implementation in spinlock.h if using multiprocessor.
// If you don't have one, set KMEM_USE_LOCK to 0.
#include "spinlock.h"
static struct spinlock kmem_lock;
#define KMEM_LOCK_INIT() initlock(&kmem_lock, "kmem")
#define KMEM_LOCK() acquire(&kmem_lock)
#define KMEM_UNLOCK() release(&kmem_lock)
#else
#define KMEM_LOCK_INIT() ((void)0)
#define KMEM_LOCK() ((void)0)
#define KMEM_UNLOCK() ((void)0)
#endif

struct run {
    struct run *next;
};

static struct run *freelist = NULL;
static size_t total_pages_count = 0;
static size_t free_pages_count = 0;
static size_t alloc_ops = 0;

/* Optionally enable KMEM_DEBUG in your build to perform slow checks */
// #define KMEM_DEBUG

#ifdef KMEM_DEBUG
static int freelist_contains(void *pa) {
    struct run *r = freelist;
    while (r) {
        if ((void*)r == pa) return 1;
        r = r->next;
    }
    return 0;
}
#endif

#define PAGE_ALIGN_DOWN(x) ((uintptr_t)(x) & ~(PGSIZE - 1))
#define PAGE_ALIGN_UP(x) (((uintptr_t)(x) + PGSIZE - 1) & ~(PGSIZE - 1))

void kinit(void *start, void *endpa) {
    KMEM_LOCK_INIT();
    uintptr_t a = PAGE_ALIGN_UP((uintptr_t)start);
    uintptr_t end = PAGE_ALIGN_DOWN((uintptr_t)endpa);

    if (a >= end) return;

    KMEM_LOCK();
    for (; a + PGSIZE <= end; a += PGSIZE) {
        struct run *r = (struct run *)a;
#ifdef KMEM_DEBUG
        memset((void*)a, 0xAB, PGSIZE);
#endif
        r->next = freelist;
        freelist = r;
        free_pages_count++;
        total_pages_count++;
    }
    KMEM_UNLOCK();
}

// return kernel-accessible page (VA) or NULL
void *kalloc(void) {
    KMEM_LOCK();
    struct run *r = freelist;
    if (r) {
        freelist = r->next;
        free_pages_count--;
        alloc_ops++;
        
        // zero to avoid leaking data
        // 必须在锁的保护下进行清零！
        memset((void*)r, 0, PGSIZE);
    }
    KMEM_UNLOCK();
    return (void*)r;
}

void kfree(void *pa) {
    if (!pa) return;

    // must be page aligned
    if (((uintptr_t)pa & (PGSIZE - 1)) != 0) {
        // in debug, you may want to panic; here we just return
        return;
    }

#ifdef KMEM_DEBUG
    KMEM_LOCK();
    if (freelist_contains(pa)) {
        // double free detected - panic or log
        // panic("kfree: double free");
        KMEM_UNLOCK();
        return;
    }
    KMEM_UNLOCK();
    // write poison
    memset(pa, 0xDB, PGSIZE);
#endif

    KMEM_LOCK();
    struct run *r = (struct run *)pa;
    r->next = freelist;
    freelist = r;
    free_pages_count++;
    KMEM_UNLOCK();
}

size_t kmem_total_pages(void) {
    size_t v;
    KMEM_LOCK();
    v = total_pages_count;
    KMEM_UNLOCK();
    return v;
}
size_t kmem_free_pages(void) {
    size_t v;
    KMEM_LOCK();
    v = free_pages_count;
    KMEM_UNLOCK();
    return v;
}
size_t kmem_alloc_count(void) {
    size_t v;
    KMEM_LOCK();
    v = alloc_ops;
    KMEM_UNLOCK();
    return v;
}
