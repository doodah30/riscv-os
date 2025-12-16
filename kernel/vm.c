// vm.c - simplified Sv39 page table helpers
#include "vm.h"
#include "memlayout.h"
#include "kmem.h"
#include "riscv.h"
#include "defs.h"
#include <stddef.h>
#include <stdint.h>

#define SATP_MODE_SV39 8UL

// allocate a zeroed page to be used as a pagetable page
static pagetable_t alloc_pagetable_page(void) {
    void *p = kalloc();
    if (!p) return NULL;
    // kalloc already zeros page
    memset(p, 0, PGSIZE);
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

void freevm(pagetable_t pagetable, uint64 sz) {
  if(pagetable == 0) return;

  if(sz > 0) {
    // 关键步骤 1: 先卸载并释放用户物理内存
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  }
  
  // 关键步骤 2: 再释放页表结构本身
  freewalk(pagetable);
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

extern char trampoline[]; 

void kvminit(void) {
  if (kernel_pagetable) return;
  kernel_pagetable = proc_pagetable_create();
  if (!kernel_pagetable) panic("kvminit: cannot alloc kernel_pagetable");

  // 1. 映射 UART0 (这个你写对了)
  // kvmmap(pt, va, pa, size, perm)
  if(kvmmap(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W) < 0)
      panic("kvminit: uart0");

  // 2. 映射 VIRTIO0 (这个也对了)
  if(kvmmap(kernel_pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W) < 0)
      panic("kvminit: virtio0");

  // 3. === 修正 PLIC ===
  // 以前：kvmmap(..., PLIC, 0x400000, PLIC, ...) <--- 错的！
  // 现在：
  if(kvmmap(kernel_pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W) < 0)
      panic("kvminit: plic");

  // 4. 映射 内核代码 (这个也对了)
  if(kvmmap(kernel_pagetable, KERNBASE, KERNBASE, (uint64)PHYSTOP - KERNBASE, PTE_R | PTE_W | PTE_X) < 0)
      panic("kvminit: kernel data");

  // 5. 映射 Trampoline (这个也对了)
  if(kvmmap(kernel_pagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X) < 0)
      panic("kvminit: trampoline");
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

// 1. 创建一个空的用户页表
// 实际上就是调用你现有的 proc_pagetable_create
pagetable_t uvmcreate() {
  pagetable_t pagetable;
  
  // 1. 分配页表页 (确保你的 alloc_pagetable_page 里有 memset 0)
  pagetable = proc_pagetable_create(); 
  if(pagetable == 0) return 0;
  // 2. === 关键修复：映射 Trampoline 到用户页表 ===
  // 这样当 satp 切换到用户页表后，CPU 依然能在高地址找到代码执行
  if(mappages(pagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline, PTE_R | PTE_X) < 0){
    // 如果失败，释放刚才分配的页表
    // freevm(pagetable, 0); // 暂时简化，直接 panic
    panic("uvmcreate: mappages trampoline");
    return 0;
  }
  
  return pagetable;
}

// 2. 加载 initcode 到用户页表的起始位置 (虚拟地址 0)
// 这是第一个用户进程诞生的关键
void uvmfirst(pagetable_t pagetable, uchar *src, uint sz) {
  char *mem;

  if(sz >= PGSIZE)
    panic("uvmfirst: more than a page");
  
  // 分配一个物理页
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  
  // 将物理页映射到虚拟地址 0
  // 权限：用户可读(R)、可写(W)、可执行(X)、用户态可访问(U)
  if(mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U) < 0) {
    panic("uvmfirst: mappages");
  }
  
  // 将代码复制到物理页中
  memmove(mem, src, sz);
}

// 3. 从用户空间复制数据到内核 (Copy In)
// 例如：系统调用 write(fd, buf, len)，内核需要从用户 buf 读取数据
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len) {
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    // walkaddr 是你代码里已经有的函数，它会检查 PTE_U 权限
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    
    // 从物理地址复制到内核 dst
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// 4. 从内核复制数据到用户空间 (Copy Out)
// 例如：系统调用 read(fd, buf, len)，内核将读取的数据填入用户 buf
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len) {
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    
    // 从内核 src 复制到物理地址
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// 为 sbrk 用：分配或释放用户内存
// 从 oldsz 调整到 newsz
// 返回新的大小，失败返回 -1 (0xff...ff)
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
  if(newsz >= oldsz) return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    // unmap_pages 是你之前写过的，这里简化调用，确保你 vm.c 里有这个逻辑
    // 或者我们手动释放
    for(uint64 a = PGROUNDUP(newsz); a < PGROUNDUP(oldsz); a += PGSIZE){
      pte_t *pte = walk(pagetable, a, 0);
      if(pte && (*pte & PTE_V)){
        uint64 pa = pte_to_pa(*pte);
        kfree((void*)PA2VA(pa));
        *pte = 0;
      }
    }
  }
  return newsz;
}

// 对应 uvmalloc (增长内存)
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// 为 fork 用：复制父进程的页表和内存到子进程
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz) {
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;
    if((*pte & PTE_V) == 0)
      continue;
    pa = pte_to_pa(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)PA2VA(pa), PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  // 发生错误，释放新分配的页面
  // unmap_pages(new, 0, i, 1); // 假设你有这个清理函数
  return -1;
}

int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte = 0; 
}

uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = pte_to_pa(*pte);
  return pa;
}

void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0) // leaf page table entry allocated?
      continue;  
    if((*pte & PTE_V) == 0)  // has physical page been allocated?
      continue;  
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = pte_to_pa(*pte);
      kfree((void*)PA2VA(pa));
    }
    *pte = 0;
  }
}

void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = pte_to_pa(pte);
      freewalk((pagetable_t)PA2VA(child));
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      printf("PANIC INFO: freewalk found leaf PTE at index %d\n", i);
      printf("PANIC INFO: PTE=%p, PA=%p\n", pte, pte_to_pa(pte));
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}