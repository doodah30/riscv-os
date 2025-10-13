// riscv.h - minimal RISC-V / Sv39 helpers and PTE macros
#ifndef RISCV_H
#define RISCV_H

#include <stdint.h>
#include <stddef.h>

#define PGSIZE 4096UL
#define PGSHIFT 12UL

typedef uint64_t pte_t;

// PTE flag bits
#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_G (1UL << 5)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)

// Sv39 specifics: 9 bits per level
static inline uint64_t vpn_index(uint64_t va, int level) {
    // level: 0 => VPN[0] (low), 1 => VPN[1], 2 => VPN[2] (high)
    return (va >> (PGSHIFT + 9 * level)) & 0x1FFUL;
}

// PTE <-> physical conversions (PTE stores PPN at bits >= 10)
static inline uint64_t pte_to_pa(pte_t pte) {
    return ((pte >> 10) << PGSHIFT);
}
static inline pte_t pa_to_pte(uint64_t pa, uint64_t flags) {
    return (((pa >> PGSHIFT) << 10) | (flags));
}

// Simple PA <-> kernel VA conversion macros.
// DEFAULT: identity (suitable for early boot / identity-mapped kernels).
// When you enable a kernel direct-map at KERNBASE, change these, e.g.:
//   #define PA2VA(pa) ((void *)((pa) + KERNBASE))
//   #define VA2PA(va) ((uint64_t)(va) - KERNBASE)
#ifndef PA2VA
#define PA2VA(pa) ((void *)(pa))
#endif
#ifndef VA2PA
#define VA2PA(va) ((uint64_t)(va))
#endif

// sfence.vma wrapper to flush TLB (for RISC-V)
static inline void sfence_vma(void) {
    // flush all entries (no ASID here)
    asm volatile("sfence.vma" ::: "memory");
}

#endif // RISCV_H
