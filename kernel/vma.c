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
    if(region->ref_cnt == 0){
      region->ref_cnt = 1;
      release(&rtable.lock);
      return region;
    }
  }
  release(&rtable.lock);
  return 0;
}

static void
vmadestroy(struct vma* region) {
  uint64 a;
  struct proc *p = myproc();
  if (region->flag == MAP_SHARED) {
    for(a = region->addr; a < region->addr + region->len; a += PGSIZE){
      pte_t *pte = walk(p->pagetable, a, 0);
      if (pte && (*pte & PTE_D)) {
        int n = min(PGSIZE, region->addr + region->len - a);
        writei(region->f->ip, 0, PTE2PA(*pte), a - region->addr, n);
      }
    }
  }
}

void
vmaclose(struct vma* region) {
  if (region == 0) {
    return;
  }

  acquire(&rtable.lock);
  region->ref_cnt--;
  if (region->ref_cnt == 0) {
    vmadestroy(region);
  }
  release(&rtable.lock);
}

struct vma* vmadup(struct vma* region) {
  if (region == 0) {
    return region;
  }

  acquire(&rtable.lock);
  if(region->ref_cnt < 1)
    panic("vmadup");
  region->ref_cnt++;
  release(&rtable.lock);

  return region;
}

void vmainit(void) {
  initlock(&rtable.lock, "vma");
}
