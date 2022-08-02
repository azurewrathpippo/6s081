#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fdp2c[2];
  int fdc2p[2];
  char buffer = 0x42;

  pipe(fdp2c);
  pipe(fdc2p);

  if (fork() == 0) { // child
    close(fdp2c[1]);
    close(fdc2p[0]);

    // print "<pid>: received ping"
    read(fdp2c[0], &buffer, 1);
    printf("%d: received ping\n", getpid());

    // write the byte on the pipe to the parent
    write(fdc2p[1], &buffer, 1);
  } else { // parent
    close(fdp2c[0]);
    close(fdc2p[1]);

    // send a byte to the child
    write(fdp2c[1], &buffer, 1);

    // print "<pid>: received pong"
    read(fdc2p[0], &buffer, 1);
    printf("%d: received pong\n", getpid());

    wait(0);
  }
  exit(0);
}
