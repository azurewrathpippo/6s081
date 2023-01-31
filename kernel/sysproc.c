#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "vma.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_mmap(void)
{
  int i;
  int len;
  int prot, flag;
  int fd;
  if(argint(1, &len) < 0)
    return -1;
  if(argint(2, &prot) < 0)
    return -1;
  if(argint(3, &flag) < 0)
    return -1;
  if(argint(4, &fd) < 0)
    return -1;

  struct proc *p = myproc();
  struct file *f = p->ofile[fd];
  if (!f || f->type != FD_INODE) {
    return -1;
  }

  // check permission
  if ((prot & PROT_READ) && !f->readable) {
    return -1;
  }
  if ((prot & PROT_WRITE) && !f->writable) {
    return -1;
  }

  struct vma *region = vmaalloc();
  if (region == 0)
    panic("no vma region");

  for (i = 0; i < NOVMA; i++) {
    if (!p->mappedregion[i]) 
      p->mappedregion[i] = region;
  }

  region->addr = getpageforvma(PGROUNDUP(len) / PGSIZE);
  region->len = len;
  region->prot = prot;
  region->flag = flag;
  region->f = filedup(f);
  region->ref_cnt = 1;

  return (uint64)region;
}

uint64
sys_munmap(void)
{
  uint64 addr;
  uint64 end_addr;
  struct proc *p;
  struct vma *region = 0;
  int i;// index of the region
  int len;
  if(argaddr(0, &addr) < 0)
    return -1;
  if(argint(1, &len) < 0)
    return -1;
  // addr must be the multiple of PGSIZE
  if (addr % PGSIZE != 0) {
    return -1;
  }
  // must unmap the whole page
  len = PGROUNDUP(len);
  // end addr
  end_addr = addr + len;
  p = myproc();
  i = getregion(addr);
  if (i == -1) {
    return -1;
  }
  region = p->mappedregion[i];

  if (addr == region->addr) {
    if (len == PGROUNDUP(region->len)) { // whole vma is unmapped
      vmaclose(region);
      p->mappedregion[i] = 0;
    } else if (len > PGROUNDUP(region->len)) {
      return -1;
    } else {
      region->addr += len;
      region->len -= addr;
    }
  } else if (end_addr == PGROUNDUP(region->addr + region->len)) {
    region->len = addr - region->addr;
  }

  return 0;
}
