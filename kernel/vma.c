#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "vma.h"
#include "defs.h"
#include "fcntl.h"
#include "proc.h"
#include "fs.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

struct {
  struct spinlock lock;
  struct vma regions[NVMA];
} rtable;

struct vma*
vmaalloc(void) {
  struct vma *region;

  acquire(&rtable.lock);
  for(region = rtable.regions; region < rtable.regions + NVMA; region++){
    if(!region->valid){
      region->valid = 1;
      release(&rtable.lock);
      return region;
    }
  }
  release(&rtable.lock);
  return 0;
}

void vmainit(void) {
  initlock(&rtable.lock, "vma");
}

int do_munmap(uint64 addr, int len) {
  uint64 a;
  struct proc *p;
  struct vma *region = 0;
  int i;// index of the region
  // addr must be the multiple of PGSIZE
  if (addr % PGSIZE != 0) {
    return -1;
  }
  // must unmap the whole page
  len = PGROUNDUP(len);
  p = myproc();
  i = getregion(addr);
  if (i == -1) {
    return -1;
  }
  region = p->mappedregion[i];

  if (addr + len > PGROUNDUP(region->addr + region->len)) {
    return -1;
  }
  // a hole in mapped region.
  if (addr > region->addr && addr + len < PGROUNDUP(region->addr + region->len)) {
    return -1;
  }

  begin_op();
  // write back the page and do unmap.
  for(a = addr; a < addr + len; a += PGSIZE){
    pte_t *pte = walk(p->pagetable, a, 0);
    if (pte && (*pte & PTE_V)) {
      // write back the page.
      if (region->flag == MAP_SHARED && (*pte & PTE_D)) {
        int n = min(PGSIZE, region->addr + region->len - a);
        writei(region->f->ip, 0, PTE2PA(*pte), a - region->addr, n);
      }
      // do unmap.
      uvmunmap(p->pagetable, a, 1, 1);
    }
  }
  if (addr == region->addr) {
    region->addr = addr + len;
    region->len -= len;
  } else if (addr + len == PGROUNDUP(region->addr + region->len)) {
    region->len = addr - region->addr;
  }
  end_op();

  if (region->len == 0) {
    p->mappedregion[i] = 0;
    fileclose(region->f);
    acquire(&rtable.lock);
    region->valid = 0;
    release(&rtable.lock);
  }

  return 0;
}