#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  printf("attack start\n");
  char *end = sbrk(PGSIZE * 32);
  //不能理解！，这个16是怎么来的？？
  printf("end: %p\n", end);
  end = end + 16 * PGSIZE;
  printf("end + 16 * PGSIZE: %p\n", end);
  char *secret = end + 32;
  printf("secret: %s\n", secret);
  write(2, secret, 8);
  exit(1);
}
