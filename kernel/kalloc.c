// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#define MAXPAGES 32768 // 假设128MB内存 (32768 * 4KB = 128MB)

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  int refcount[MAXPAGES];
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}
// 获取物理地址对应的引用计数数组索引
static int
pa2index(uint64 pa)
{
  if (pa < (uint64)end)
    return -1;
  uint64 idx = (pa - (uint64)end) / PGSIZE;
  if (idx >= MAXPAGES)
    return -1;
  return idx;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  int count = 0;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    count++;
    kmem.refcount[pa2index((uint64)p)] = 1;
    kfree(p);
  }
  printf("freerange: initialized %d pages\n", count);
}


//增加引用计数
void
inc_ref(void *pa){
  int idx = pa2index((uint64)pa);
  if (idx < 0)
    return;

  acquire(&kmem.lock);
  kmem.refcount[idx]++;
  // if(idx<10000)
  //   printf("inc ref at pa %ld,idx is %d,refcount is %d\n ", (uint64)pa,idx, kmem.refcount[idx]);
  release(&kmem.lock);
}

// 获取引用计数
int get_ref(void *pa)
{
  int idx = pa2index((uint64)pa);
  int count;
  if (idx < 0)
    return -1;

  acquire(&kmem.lock);
  count=kmem.refcount[idx];
  release(&kmem.lock);
  return count;
}
// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  int idx = pa2index((uint64)pa);
  if (idx < 0 || kmem.refcount[idx] <= 0){
    printf("end is %lx,pa is %lx,idx is %d,ref count is %d\n", (uint64)end,(uint64)pa, idx, kmem.refcount[idx]);
    panic("kfree: invalid pa or refcount");
  }
  acquire(&kmem.lock);
  int count = --kmem.refcount[idx];
  // Fill with junk to catch dangling refs.
  if(count==0)
  {
    memset(pa, 1, PGSIZE);
    r = (struct run *)pa;
    r->next = kmem.freelist;
    kmem.freelist = r; 
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    kmem.refcount[pa2index((uint64)r)] = 1; // 设置引用计数为1
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
