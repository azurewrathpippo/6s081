#include "kernel/types.h"
#include "user/user.h"

#define WRITE_ENDPOINT 1
#define READ_ENDPOINT 0

void work(int inputfd) {
  int base = -1; // base num(for main, base should be 2)
  int child_created = 0;
  int input;
  int pipefd[2];

  while(read(inputfd, &input, sizeof(input)) != 0) {
    if (base == -1) { // base must be a prime
      base = input;
      printf("prime %d\n", base);
      continue;
    }
    if (input % base == 0) {
      continue;
    }
    if (!child_created) { // create child
      pipe(pipefd);
      if (fork() == 0) { // child
        close(pipefd[WRITE_ENDPOINT]);
        work(pipefd[READ_ENDPOINT]);
        exit(0);
      } else { // parent
        close(pipefd[READ_ENDPOINT]);
        child_created = 1;
      }
    }
    write(pipefd[WRITE_ENDPOINT], &input, sizeof(input));
  }
  close(pipefd[WRITE_ENDPOINT]);
  if (child_created) {
    wait(0);
  }
}

int
main(int argc, char *argv[])
{
  int pipefd[2];
  pipe(pipefd);
  for (int i = 2; i <= 35; i++) {
    write(pipefd[WRITE_ENDPOINT], &i, sizeof(i));
  }
  close(pipefd[WRITE_ENDPOINT]);
  work(pipefd[READ_ENDPOINT]);
  exit(0);
}
