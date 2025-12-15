// vm.h - page table API (Sv39 simplified)
#ifndef VM_H
#define VM_H

#include "riscv.h"
#include <stdint.h>

// pagetable_t is a pointer to a page-sized array of pte_t

extern pagetable_t kernel_pagetable;

pagetable_t proc_pagetable_create(void); // alloc and zero a root pagetable page
void proc_pagetable_free(pagetable_t pagetable); // free all pages (and page table pages)

pte_t *walk(pagetable_t pagetable, uint64_t va, int alloc);
int mappages(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, int perm);
void unmap_pages(pagetable_t pagetable, uint64_t va, uint64_t size); // unmap and free physical pages
uint64_t walkaddr(pagetable_t pagetable, uint64_t va); // get PA mapped by va (or 0)

uint64 uvmalloc(pagetable_t, uint64, uint64, int xperm);
uint64 uvmdealloc(pagetable_t, uint64, uint64);
pagetable_t copyuvm(pagetable_t old, uint64_t sz);
void freevm(pagetable_t pagetable, uint64_t sz);
void print_pagetable(pagetable_t root);
void kvminit(void);
void kvminithart(void);
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz);
#endif // VM_H
