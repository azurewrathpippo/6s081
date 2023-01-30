struct vma {
  struct spinlock lock;

  uint addr;// 0 represent invalid.
  int len;
  int prot;
  int flag;
  int fd;
  int ref_cnt;
};
