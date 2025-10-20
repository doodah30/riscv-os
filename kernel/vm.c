// vm.c - simplified Sv39 page table helpers
#include "vm.h"
#include "memlayout.h"
#include "kmem.h"
#include "riscv.h"
#include "defs.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#define SATP_MODE_SV39 8UL

// allocate a zeroed page to be used as a pagetable page
static pagetable_t alloc_pagetable_page(void) {
    void *p = kalloc();
    if (!p) return NULL;
    // kalloc already zeros page
    return (pagetable_t)p;
}

pagetable_t proc_pagetable_create(void) {
    pagetable_t p = alloc_pagetable_page();
    return p;
}

//static void free_pagetable_page(void *page) {
//    kfree(page);
//}

// walk: return pointer to PTE for va; if alloc and missing, allocate intermediate page table pages
pte_t *walk(pagetable_t pagetable, uint64_t va, int alloc) {
    pte_t *pte;
    pagetable_t p = pagetable;
    for (int level = 2; level > 0; level--) {
        uint64_t idx = vpn_index(va, level);
        pte = &p[idx];
        if (*pte & PTE_V) {
            // non-leaf -> next level: extract next page physical address
            uint64_t next_pa = pte_to_pa(*pte);
            p = (pagetable_t)PA2VA(next_pa);
        } else {
            if (!alloc) return NULL;
            pagetable_t newpage = alloc_pagetable_page();
            if (!newpage) return NULL;
            // link it in: mark valid and point to it
            uint64_t newpa = VA2PA(newpage);
            *pte = pa_to_pte(newpa, PTE_V);
            p = newpage;
        }
    }
    // level 0
    uint64_t idx0 = vpn_index(va, 0);
    return &p[idx0];
}

// mappages: map [va, va+size) to [pa, pa+size)
int mappages(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, int perm) {
    uint64_t a = va & ~(PGSIZE - 1);
    uint64_t last = ((va + size - 1) & ~(PGSIZE - 1));
    for (; a <= last; a += PGSIZE, pa += PGSIZE) {
        pte_t *pte = walk(pagetable, a, 1);
        if (!pte) return -1;
        if (*pte & PTE_V) {
            // already mapped
            return -1;
        }
        *pte = pa_to_pte(pa, perm | PTE_V);
    }
    // after mapping, flush TLB for safety in current hart
    sfence_vma();
    return 0;
}

// unmap pages and free the physical pages mapped
void unmap_pages(pagetable_t pagetable, uint64_t va, uint64_t size) {
    uint64_t a = va & ~(PGSIZE - 1);
    uint64_t last = ((va + size - 1) & ~(PGSIZE - 1));
    for (; a <= last; a += PGSIZE) {
        pte_t *pte = walk(pagetable, a, 0);
        if (!pte) continue;
        if (!(*pte & PTE_V)) continue;
        uint64_t pa = pte_to_pa(*pte);
        // clear entry
        *pte = 0;
        kfree((void *)PA2VA(pa));
    }
    sfence_vma();
}

// walkaddr: return physical address for va (or 0 if not mapped)
uint64_t walkaddr(pagetable_t pagetable, uint64_t va) {
    pte_t *pte = walk(pagetable, va, 0);
    if (!pte) return 0;
    if (!(*pte & PTE_V)) return 0;
    // must be leaf (R/W/X bits indicate leaf)
    if (!((*pte & PTE_R) || (*pte & PTE_X) || (*pte & PTE_W))) return 0;
    return pte_to_pa(*pte) | (va & (PGSIZE - 1));
}

// uvmalloc: allocate pages to grow from oldsz to newsz (both bytes)
int uvmalloc(pagetable_t pagetable, uint64_t oldsz, uint64_t newsz) {
    if (newsz < oldsz) return -1;
    uint64_t a = (oldsz + PGSIZE - 1) & ~(PGSIZE - 1);
    for (; a + PGSIZE <= newsz; a += PGSIZE) {
        void *mem = kalloc();
        if (!mem) {
            // allocation failure: rollback previously allocated pages
            unmap_pages(pagetable, (oldsz + PGSIZE - 1) & ~(PGSIZE - 1), a - ((oldsz + PGSIZE - 1) & ~(PGSIZE - 1)));
            return -1;
        }
        uint64_t pa = VA2PA(mem);
        if (mappages(pagetable, a, PGSIZE, pa, PTE_R | PTE_W | PTE_U) != 0) {
            kfree(mem);
            unmap_pages(pagetable, (oldsz + PGSIZE - 1) & ~(PGSIZE - 1), a - ((oldsz + PGSIZE - 1) & ~(PGSIZE - 1)));
            return -1;
        }
    }
    return 0;
}

void uvmdealloc(pagetable_t pagetable, uint64_t oldsz, uint64_t newsz) {
    if (newsz >= oldsz) return;
    uint64_t a = ((newsz + PGSIZE - 1) & ~(PGSIZE - 1));
    for (; a + PGSIZE <= oldsz; a += PGSIZE) {
        // unmap_pages will free the physical page
        unmap_pages(pagetable, a, PGSIZE);
    }
}

// helper to walk pagetable recursively and free page-table pages and leaf pages
static void free_pagetable_recursive(pagetable_t p, int level) {
    if (!p) return;
    for (int i = 0; i < 512; i++) {
        pte_t ent = p[i];
        // 只关心指向下一级页表的PTE，忽略叶子PTE
        if ((ent & PTE_V) && (ent & (PTE_R | PTE_W | PTE_X)) == 0) {
            // non-leaf -> recurse
            uint64_t child_pa = pte_to_pa(ent);
            pagetable_t child = (pagetable_t)PA2VA(child_pa);
            free_pagetable_recursive(child, level - 1);
            // p[i] = 0; // 这一行可有可无，因为马上就要释放 p 了
        }
    }
    // free this page table page itself
    kfree((void *)p);
}

void freevm(pagetable_t pagetable, uint64_t sz) {
    if (!pagetable) return;
    // free all mapped pages and page-table pages
    free_pagetable_recursive(pagetable, 2);
}

// copyuvm: copy user memory from old pagetable into a newly allocated pagetable
pagetable_t copyuvm(pagetable_t old, uint64_t sz) {
    pagetable_t new = proc_pagetable_create();
    if (!new) return NULL;
    // walk each page in user space and copy
    for (uint64_t i = 0; i < sz; i += PGSIZE) {
        uint64_t pa = walkaddr(old, i);
        if (pa == 0) {
            // page not present -> skip (sparse)
            continue;
        }
        void *mem = kalloc();
        if (!mem) {
            // allocation fail -> cleanup
            freevm(new, i);
            return NULL;
        }
        // copy content
        memcpy(mem, PA2VA(pa), PGSIZE);
        uint64_t mem_pa = VA2PA(mem);
        if (mappages(new, i, PGSIZE, mem_pa, PTE_R | PTE_W | PTE_U) != 0) {
            kfree(mem);
            freevm(new, i);
            return NULL;
        }
    }
    return new;
}

pagetable_t kernel_pagetable = NULL;

/* helper: wrapper to call mappages for kernel mapping convenience */
static int kvmmap(pagetable_t pt, uint64_t va, uint64_t pa, uint64_t size, int perm) {
    return mappages(pt, va, size, pa, perm);
}

/* print PTE flags as string */
static void print_pte_flags(pte_t pte) {
    char f[9];
    int j = 0;
    f[j++] = (pte & PTE_V) ? 'V' : '-';
    f[j++] = (pte & PTE_R) ? 'R' : '-';
    f[j++] = (pte & PTE_W) ? 'W' : '-';
    f[j++] = (pte & PTE_X) ? 'X' : '-';
    f[j++] = (pte & PTE_U) ? 'U' : '-';
    f[j++] = (pte & PTE_G) ? 'G' : '-';
    f[j++] = (pte & PTE_A) ? 'A' : '-';
    f[j++] = (pte & PTE_D) ? 'D' : '-';
    f[j] = '\0';
    printf("%s", f);
}

/* level_size: level 0 -> 4KB, level 1 -> 2MB, level 2 -> 1GB */
static inline uint64_t level_size(int level) {
    return 1UL << (PGSHIFT + 9 * level);
}

/* print indent */
static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) consputc(' ');
}

void print_pagetable_recursive(pagetable_t pagetable, int level, uint64_t va_base, int depth) {
    if (!pagetable) return;
    for (int i = 0; i < 512; i++) {
        pte_t ent = pagetable[i];
        if (!(ent & PTE_V)) continue;

        uint64_t this_va = va_base + ((uint64_t)i << (PGSHIFT + 9 * level));

        /* If this is a leaf PTE (has R/W/X) OR we're at level 0, treat as leaf */
        if ((ent & (PTE_R | PTE_W | PTE_X)) != 0 || level == 0) {
            uint64_t pa = pte_to_pa(ent);
            uint64_t sz = level_size(level); // level==0 -> 4KB
            print_indent(depth);
            printf("LEAF: VA 0x%llx - 0x%llx => PA 0x%llx size 0x%llx flags: ",
                   (unsigned long long)this_va,
                   (unsigned long long)(this_va + sz - 1),
                   (unsigned long long)pa,
                   (unsigned long long)sz);
            print_pte_flags(ent);
            printf("\n");
        } else {
            /* Non-leaf and level > 0: compute the range correctly and recurse */
            uint64_t pa = pte_to_pa(ent);
            if (level <= 0) {
                /* Defensive: should not happen, but avoid negative recursion */
                print_indent(depth);
                printf("NODE (unexpected at level 0): VA 0x%llx -> child PA 0x%llx\n",
                       (unsigned long long)this_va, (unsigned long long)pa);
                continue;
            }
            uint64_t size = level_size(level); /* size covered by this entry */
            print_indent(depth);
            printf("NODE : VA range 0x%llx - 0x%llx -> child PA 0x%llx\n",
                   (unsigned long long)this_va,
                   (unsigned long long)(this_va + size - 1),
                   (unsigned long long)pa);

            pagetable_t child = (pagetable_t)PA2VA(pa);
            print_pagetable_recursive(child, level - 1, this_va, depth + 2);
        }
    }
}


/* wrapper to print from root */
void print_pagetable(pagetable_t root) {
    if (!root) {
        printf("print_pagetable: root is NULL\n");
        return;
    }
    printf("=== print_pagetable root=%p ===\n", root);
    print_pagetable_recursive(root, 2, 0, 0);
    printf("=== end print_pagetable ===\n");
}

/* kvminit: build kernel pagetable (but do not write satp) */
void kvminit(void) {
    if (kernel_pagetable) return;
    kernel_pagetable = proc_pagetable_create();
    if (!kernel_pagetable) panic("kvminit: cannot alloc kernel_pagetable");

    /* map devices: UART0 */
#ifdef UART0
    kvmmap(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);
#endif
// 映射 VIRTIO 磁盘寄存器
#ifdef VIRTIO0
    kvmmap(kernel_pagetable, VIRTIO0, PGSIZE, VIRTIO0, PTE_R | PTE_W);
#endif

    // 映射 PLIC 寄存器
#ifdef PLIC
    kvmmap(kernel_pagetable, PLIC, 0x400000, PLIC, PTE_R | PTE_W);
#endif

    // 映射 CLINT 寄存器
#ifdef CLINT
    // CLINT 区域很小，映射一个页面就足够了
    kvmmap(kernel_pagetable, CLINT, 0x10000, CLINT, PTE_R | PTE_W);
#endif
    /* identity-map kernel physical memory [KERNBASE, PHYSTOP) */
#ifdef KERNBASE
#ifdef PHYSTOP
    kvmmap(kernel_pagetable, KERNBASE, KERNBASE, PHYSTOP - KERNBASE, PTE_R | PTE_W | PTE_X);
#else
#warning "PHYSTOP not defined; kernel physical memory mapping skipped"
#endif
#else
#warning "KERNBASE not defined; kernel memory mapping skipped"
#endif
}

/* kvminithart: write kernel_pagetable -> satp and sfence.vma */
void kvminithart(void) {
    if (!kernel_pagetable) panic("kvminithart: kernel_pagetable not initialized");
    uint64_t root_pa = VA2PA((uint64_t)kernel_pagetable);
    uint64_t root_ppn = root_pa >> PGSHIFT;
    uint64_t satp_val = (SATP_MODE_SV39 << 60) | root_ppn;
    asm volatile("csrw satp, %0" :: "r"(satp_val) : "memory");
    asm volatile("sfence.vma" ::: "memory");
}