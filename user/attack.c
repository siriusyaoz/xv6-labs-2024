#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  char *end = sbrk(PGSIZE * 32);
  //不能理解！，这个16是怎么来的？？
  end = end + 16 * PGSIZE;
  char *secret = end + 32;
  printf("secret: %s\n", secret);
  write(2, secret, 8);
  exit(1);
}
