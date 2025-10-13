// kmem.h - physical page allocator API
#ifndef KMEM_H
#define KMEM_H

#include <stddef.h>
#include <stdint.h>

void kinit(void *start, void *endpa); // start/end are kernel-accessible addresses (VA)
void *kalloc(void);                    // return a kernel-accessible page (VA) or NULL
void kfree(void *pa);                  // pa is the pointer returned by kalloc (VA)

size_t kmem_total_pages(void);
size_t kmem_free_pages(void);
size_t kmem_alloc_count(void);

#endif // KMEM_H
