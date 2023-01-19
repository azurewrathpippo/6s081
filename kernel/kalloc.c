// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void kfree_cpu(void *pa, int cpu_index);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    char buffer[32];
    snprintf(buffer, 32, "kmem[%d]", i);
    initlock(&kmem[i].lock, buffer);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  int page_count = ((char *)pa_end - p) / PGSIZE;
  for (int i = 0; i < NCPU; i++) {
    int first_page = page_count / NCPU * i;
    int last_page = page_count / NCPU * (i + 1) - 1;
    if (i == NCPU - 1) {
      last_page = page_count - 1;
    }
    for (int j = first_page; j <= last_page; j++) {
      kfree_cpu(p + PGSIZE * j, i);
    }
  }
}

void
kfree_cpu(void *pa, int cpu_index)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpu_index].lock);
  r->next = kmem[cpu_index].freelist;
  kmem[cpu_index].freelist = r;
  release(&kmem[cpu_index].lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  int cpu_index;
  push_off();
  cpu_index = cpuid();
  pop_off();

  kfree_cpu(pa, cpu_index);
}

void *
kalloc_cpu(int cpu_index)
{
  struct run *r;

  acquire(&kmem[cpu_index].lock);
  r = kmem[cpu_index].freelist;
  if(r)
    kmem[cpu_index].freelist = r->next;
  release(&kmem[cpu_index].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  int cpu_index;
  push_off();
  cpu_index = cpuid();
  pop_off();

  for (int i = 0; i < NCPU; i++) {
    void *r = kalloc_cpu((cpu_index + i) % NCPU);
    if (r) {
      return r;
    }
  }

  return 0;
}
