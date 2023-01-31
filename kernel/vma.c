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
