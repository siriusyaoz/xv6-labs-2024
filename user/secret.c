#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"


int
main(int argc, char *argv[])
{
  if(argc != 2){
    printf("Usage: secret the-secret\n");
    exit(1);
  }
  int size=PGSIZE*32;
  printf("size: %d\n", size);
  char *end = sbrk(size);
  end = end + 9 * PGSIZE;
  
  strcpy(end, "my very very very secret pw is:   ");
  strcpy(end+32, argv[1]);
  printf("end: %s pointer %p\n", end, end);
  exit(0);
}

