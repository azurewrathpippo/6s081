struct file;

struct vma {
  uint64 addr;// 0 represent invalid.
  int len;
  int prot;
  int flag;
  struct file *f;
  int ref_cnt;
};
