// spinlock.h - minimal spinlock using gcc atomic builtins
#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

struct spinlock {
    volatile uint32_t locked; // 0 = free, 1 = locked
    const char *name;
};

static inline void initlock(struct spinlock *lk, const char *name) {
    lk->locked = 0;
    lk->name = name;
}

// acquire lock (spin-wait)
static inline void acquire(struct spinlock *lk) {
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0) {
        // busy wait; a "pause" hint could be inserted for x86 with asm("pause");
        // On RISC-V you can optionally insert "wfi" or a small backoff.
        asm volatile("nop");
    }
    // got lock
}

// release lock
static inline void release(struct spinlock *lk) {
    __sync_lock_release(&lk->locked); // sets to 0
}

#endif // SPINLOCK_H
