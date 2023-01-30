#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "vma.h"
#include "defs.h"

struct vma regions[NVMA];

struct vma*
vmaalloc(void) {
  struct vma *region;

  for(region = regions; region < regions + NVMA; region++){
    acquire(&region->lock);
    if(region->ref_cnt == 0){
      region->ref_cnt = 1;
      release(&region->lock);
      return region;
    }
    release(&region->lock);
  }
  return 0;
}

static void
vmadestroy(struct vma* region) {
  //TODO
}

void
vmaclose(struct vma* region) {
  if (region == 0) {
    return;
  }

  acquire(&region->lock);
  region->ref_cnt--;
  if (region->ref_cnt == 0) {
    vmadestroy(region);
  }
  release(&region->lock);
}

struct vma* vmadup(struct vma* region) {
  if (region == 0) {
    return region;
  }

  acquire(&region->lock);
  if(region->ref_cnt < 1)
    panic("vmadup");
  region->ref_cnt++;
  release(&region->lock);

  return region;
}

void vmainit(void) {
  struct vma *region;
  
  for(region = regions; region < regions + NVMA; region++){
      initlock(&region->lock, "vma");
  }
}
