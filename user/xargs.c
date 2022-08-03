#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

// first should be the command to be executed
void xargs_exec(char *full_argv[]) {
  if (fork() == 0) { // child
    exec(full_argv[0], full_argv);
  } else { // parent
    wait(0);
  }
}

int
main(int argc, char *argv[])
{
  char *full_argv[MAXARG];
  char input;
  char buf[512];
  int index = 0;// next pos where input should be

  memcpy(full_argv, argv, argc * sizeof(char *));
  full_argv[argc] = buf;
  full_argv[argc + 1] = 0;

  while(read(0, &input, sizeof(input)) != 0) {
    if (input == '\n') {
      buf[index++] = 0;
      if (index > 1) {
        xargs_exec(full_argv + 1);
      }
      index = 0;
    } else {
      buf[index++] = input;
      if (index == 512) { // line too long
        fprintf(2, "xargs: line too long...");
        exit(1);
      }
    }
  }

  buf[index] = 0;
  if (index > 1) {
    xargs_exec(full_argv + 1);
  }
   
  exit(0);
}
