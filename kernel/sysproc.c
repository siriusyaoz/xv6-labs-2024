#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
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

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
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

  argint(0, &pid);
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
  struct proc *p = myproc();
  struct vma *ptvma=0;
  for(int i=0;i < 16; i++){
    if(p->pvma[i].npages==0){
      ptvma = &(p->pvma[i]);
      printf("mmap at pvma %d\n",i);
      break;
    }
  }
  if(!ptvma){
    printf("no enough vmas\n");
    return -1;
  }
  p->sz=PGROUNDUP(p->sz);
  ptvma->addr= p->sz;
  argsizet(1, &ptvma->len);//len is type size_t
  argint(2, &ptvma->prot);
  argint(3, &ptvma->flags);
  argint(4, &ptvma->fd);//offset is type long
  arglong(5, &ptvma->offset);
  ptvma->vfile = p->ofile[ptvma->fd];
  //check file write/read permissions.
  //check write only when map_shared
  if ((ptvma->prot & PROT_READ) && !((ptvma->vfile)->readable))
    return 0xffffffffffffffff;
  if ((ptvma->flags & MAP_SHARED) && (ptvma->prot & PROT_WRITE) && !((ptvma->vfile)->writable))
    return 0xffffffffffffffff;

  filedup(ptvma->vfile);
  ptvma->npages=PGROUNDUP(ptvma->len)/PGSIZE;
  p->sz += ptvma->npages*PGSIZE;
  // if (growproc(ptvma->len) < 0)
  //   return -1;
  // fileread(ptvma->vfile, (uint64)ptvma->addr, ptvma->len);
  return ptvma->addr;
}
uint64
sys_munmap(void)
{
  uint64 addr;
  size_t len;
  struct proc *p = myproc();
  struct vma *ptvma;
  argaddr(0,&addr);
  argsizet(1, &len); // len is type size_t
  int i = 0;
  for(; i<16; i++){
    ptvma = &(p->pvma[i]);
    printf("munmap addr is %lx,ptvma->addr is %lx,len is %lx,ptvma->len is %lx ,ptvma->npages is %d,ptvma->offset is %lx\n",
           addr, ptvma->addr, len, ptvma->len, ptvma->npages, ptvma->offset);
    if (addr >= ptvma->addr&& addr < ptvma->addr + ptvma->len)
    {
      //int npages=PGROUNDUP(len)/PGSIZE;
      munmap(i,p,addr,len);
      break;
    }
  }
  if(i==16){
    return -1;
  }
  return 0;
}

// write a page if it's mapped. n is pgsize or file's residue size
int 
munmap_filewrite(struct file *f, uint64 addr, uint off, int n)
{
  int r, ret = 0;
  int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;
  int i = 0;
  while (i < n)
  {
    int n1 = n - i;
    if (n1 > max)
      n1 = max;

    begin_op();
    ilock(f->ip);
    printf("file size is %d,off is %d,n1 is %d,addr is %lx,i is %x\n",f->ip->size,off,n1,addr,i);
    if ((r = writei(f->ip, 1, addr + i, off, n1)) > 0)
      off += r;
    else{
      printf("write r=%d bytes\n",r);
    }
    iunlock(f->ip);
    end_op();

    if (r != n1)
    {
      // error from writei
      break;
    }
    i += r;
  }
  ret = (i == n ? n : -1);
  printf("mfilewrite called with ret %d,n is %d,i is %d\n",ret,n,i);
  return ret;
}

int 
munmap(int i, struct proc *p, uint64 addr,int len)
{
  int do_free = 0;
  struct vma *ptvma = &(p->pvma[i]);
  struct file* vfile=ptvma->vfile;
  uint fizesize = vfile->ip->size;
  uint64 va = addr;
  int npages = PGROUNDUP(len) / PGSIZE;
  int can_write=(ptvma->flags & MAP_SHARED) && (ptvma->prot & PROT_WRITE) &&(vfile->writable);
    //不能直接使用filewrite,这样可能会改变file的size，造成错误的写入
    //filewrite(ptvma->vfile, addr, len);

  for (int j = 0; j < npages; j++)
  {
    if(walkaddr(p->pagetable,va)!=0){
      if(can_write){
        int off= va-addr;
        int n = (off + PGSIZE + ptvma->offset > fizesize) ? fizesize % PGSIZE : PGSIZE;
        if(munmap_filewrite(vfile,va,ptvma->offset + off, n)==-1){
          printf("munmap_filewrite error\n");
          return -1;
        };
      }
      //uvmunmap要放到上面的if 后面。不然page table entry不指向物理地址
      uvmunmap(p->pagetable, va, 1, do_free);
    }
    va += PGSIZE;
  }
  //当munmap的位置在开头时，要对VMA做特别调整
  if (addr == ptvma->addr)
  {
    ptvma->addr = va;
    ptvma->offset += npages * PGSIZE;
  }
  ptvma->npages -= npages;
  ptvma->len  -=  len;
  if (ptvma->npages == 0)
  {
    // do_free=1;
    //ref cnt -1
    fileclose(ptvma->vfile);
    ptvma->vfile = 0;
  };
  return 0;
}

