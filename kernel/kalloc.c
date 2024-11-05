// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  char name[8];
} kmem[NCPU];

#define BATCH_SIZE 3
void
kinit()
{
  for(int i=0;i<NCPU;i++){
    snprintf(kmem[i].name, 6, "kmem%d", i);
    initlock(&kmem[i].lock, kmem[i].name);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();
  int cpu_id = cpuid();
  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  // if(cpu_id)
  //   printf("kfree in cpuid %d\n",cpu_id);
  release(&kmem[cpu_id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;//,*fast, *slow;
  //struct run *head;
  struct run *tail;
  //int count;
  push_off();
  int cpu_id=cpuid();
  pop_off();
  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if(r){
    kmem[cpu_id].freelist = r->next;
    release(&kmem[cpu_id].lock);
    }
  else{
    release(&kmem[cpu_id].lock);
    //printf("cpu id %d freelist empty ",cpu_id);
    int i=(cpu_id+1)%NCPU;
    for(;i!=cpu_id;i=(i+1)%NCPU){
    // for (int i = 0; i <NCPU; i++)
    // {
    //   if(i==cpu_id){continue;}

      // steal half everytime
      // acquire(&kmem[i].lock);
      // r = kmem[i].freelist;
      // if(r){
      //   fast=r;
      //   slow=r;
      //   while(fast && fast->next){
      //     slow=slow->next;
      //     fast=fast->next->next;
      //   }
      //   kmem[i].freelist = slow->next;

      //   release(&kmem[i].lock);
      //   slow->next = 0;
      //   acquire(&kmem[cpu_id].lock);
      //   kmem[cpu_id].freelist = r->next;
      //   release(&kmem[cpu_id].lock);
      //   break;
      // }
      // release(&kmem[i].lock);

      // release(&kmem[i].lock);
      // memset((char *)r, 5, PGSIZE); // fill with junk
      // return (void *)r;

      //steal one everytime
      // acquire(&kmem[i].lock);
      // r = kmem[i].freelist;
      // if(r){
      //   kmem[i].freelist=r->next;
      //   release(&kmem[i].lock);
      //   break;
      // }
      // release(&kmem[i].lock);

      //steal 2 or 3 everytime
      acquire(&kmem[i].lock);
      if (kmem[i].freelist)
      {
        //printf("malloced from cpuid %d.\n",i);
        // Steal multiple pages in one go
        r = kmem[i].freelist;
        tail = r;
        // count = 1;

        // // Find a few pages to steal (e.g., 2-3 pages)
        //change while to for improves the performance??
        // while (count < BATCH_SIZE && tail->next)
        // {
        //   tail = tail->next;
        //   count++;
        // }
        for(int count=1;count < BATCH_SIZE && tail->next;count++){
          tail = tail->next;
        }
        // Detach stolen pages from source CPU
        kmem[i].freelist = tail->next;
        release(&kmem[i].lock);
        tail->next = 0;

        // Add to local freelist
        acquire(&kmem[cpu_id].lock);
         // First page to return
        kmem[cpu_id].freelist = r->next;
        release(&kmem[cpu_id].lock);
        break;
      }
      release(&kmem[i].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
