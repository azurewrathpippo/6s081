#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// remove suffix slash
void erase_path_suffix(char *path) {
  int len = strlen(path);
  if (path[len - 1] == '/') {
    path[len - 1] = '\0';
  }
}

void find(char *path, const char *filename) {
  erase_path_suffix(path);

  char buf[512], *p; // buf is the sub path
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    fprintf(2, "find: first parameter should be a directory %s\n", path);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0 ) { // ignore . and ..
        continue;
      }
      memmove(p, de.name, DIRSIZ);
      if (strcmp(de.name, filename) == 0) { // find file success
        printf("%s\n", buf);
      }
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      if (st.type == T_DIR) {
        find(buf, filename);
      }
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc != 3){
    fprintf(2, "Usage: find files...\n");
    exit(1);
  }
  char path[512];
  strcpy(path, argv[1]);
  find(path, argv[2]);
  exit(0);
}
