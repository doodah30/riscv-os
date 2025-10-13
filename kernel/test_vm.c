// test_vm.c - vm & allocator tests (fixed types)
#include "types.h"
#include "riscv.h"
#include "kmem.h"
#include "vm.h"
#include "memlayout.h"
#include "defs.h"   // printf, panic
#include <stdint.h> // uint64_t
#include <stddef.h>

/* wrappers to keep names short */
void *alloc_page(void) { return kalloc(); }
void free_page(void *p) { kfree(p); }

//debug用的函数
void debug_dump_pte_for_va(uint64_t va) {
    pte_t *pte = walk(kernel_pagetable, va, 0);
    if (!pte) {
        printf("debug: no pte for va 0x%llx\n", (unsigned long long)va);
        return;
    }
    uint64_t ent = *pte;
    uint64_t ppn = ent >> 10;
    uint64_t pa = ppn << 12;
    printf("debug: pte for va 0x%llx => raw=0x%llx, pa=0x%llx, flags=0x%llx\n",
           (unsigned long long)va, (unsigned long long)ent,
           (unsigned long long)pa, (unsigned long long)(ent & 0x3FF));
}
/* Test 1: physical allocator basic */
void test_physical_memory(void) {
    printf("=== test_physical_memory ===\n");
    void *page1 = alloc_page();
    void *page2 = alloc_page();
    if (!page1 || !page2) panic("alloc_page failed");
    if (page1 == page2) panic("alloc returned same page twice");
    if (((uint64_t)page1 & (PGSIZE - 1)) != 0) panic("page1 not page-aligned");
    *(volatile int*)page1 = 0x12345678;
    if (*(volatile int*)page1 != 0x12345678) panic("mem write/read mismatch");
    free_page(page1);
    void *page3 = alloc_page();
    if (!page3) panic("alloc_page failed (2)");
    free_page(page2);
    free_page(page3);
    printf("physical allocator test OK\n");
}

/* Test 2: pagetable mapping & lookup */
void test_pagetable(void) {
    printf("=== test_pagetable ===\n");
    pagetable_t pt = proc_pagetable_create();
    if (!pt) panic("create_pagetable failed");
    uint64_t va = 0x1000000UL;
    void *p = alloc_page();
    if (!p) panic("alloc_page failed for pt test");
    uint64_t pa = VA2PA((uint64_t)p);
    if (mappages(pt, va, PGSIZE, pa, PTE_R | PTE_W) != 0) panic("map_page failed");
    pte_t *pte = walk(pt, va, 0);
    if (!pte || !(*pte & PTE_V)) panic("walk_lookup failed or not valid");
    if (pte_to_pa(*pte) != pa) panic("mapped PA mismatch");
    if (!(*pte & PTE_R) || !(*pte & PTE_W)) panic("permission bits wrong");
    if ((*pte & PTE_X)) panic("execute bit set unexpectedly");
    /* cleanup */
    unmap_pages(pt, va, PGSIZE);
    //kfree(p);//重大bug
    freevm(pt, PGSIZE);
    printf("pagetable test OK\n");
}

/* Test 3: enable kernel vm on current hart (kvminit + kvminithart) */
void test_virtual_memory(void) {
    printf("=== test_virtual_memory ===\n");
    printf("Before kvminit, kernel_pagetable = %p\n", kernel_pagetable);
    kvminit();

    printf("After kvminit, kernel_pagetable = %p\n", kernel_pagetable);
    /* enable on this hart */
    kvminithart();
    printf("kvminithart done. (paging enabled on this hart)\n");
#ifdef KERNBASE
    /* try a simple read on KERNBASE (must be mapped by kvminit) */
    volatile uint64_t tmp = *(volatile uint64_t *)KERNBASE;
    (void)tmp;
    printf("Read at KERNBASE succeeded (0x%llx)\n", (unsigned long long)tmp);
#else
    printf("KERNBASE not defined; skipping KERNBASE read\n");
#endif
    printf("virtual memory activation test OK\n");
}

/* convenience entry to run all tests */
void run_vm_tests(void) {
    test_physical_memory();
    test_pagetable();
    test_virtual_memory();
    /* print kernel pagetable for inspection (after kvminit) */
    if (kernel_pagetable) {
        print_pagetable(kernel_pagetable);
    }
}
