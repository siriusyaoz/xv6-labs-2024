#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13 // 哈希表大小，使用素数13
#define HASH(blockno) ((blockno)%NBUCKETS)
struct bucket
{
    struct spinlock lock; // 每个bucket自己的锁
    struct buf *head;     // 链表头
    char name[16];
};
struct
{
    struct bucket bucket[NBUCKETS];
    struct spinlock lock;
    struct buf buf[NBUF];
} bcache;

void binit(void)
{
    struct buf *b;
    initlock(&bcache.lock, "bcache");
    for (int i = 0; i < NBUCKETS; i++)
    {
        snprintf(bcache.bucket[i].name, 16, "bcache_bucket%d", i);
        printf("bucket name is %s\n",bcache.bucket[i].name);
        initlock(&bcache.bucket[i].lock, bcache.bucket[i].name);
    }
    for (b = bcache.buf; b < bcache.buf + NBUF; b++)
    {
        initsleeplock(&b->lock, "buffer");
        b->next= bcache.bucket[0].head;
        bcache.bucket[0].head=b;
        //printf("buf refcnt is %d\n",b->refcnt);
    }
}
static struct buf *
bget(uint dev, uint blockno)
{
    
    struct buf *b,*prev=0;
    //acquire(&bcache.lock);

    // Is the block already cached?
    int key= HASH(blockno);
    acquire(&bcache.bucket[key].lock);
    b = bcache.bucket[key].head;
    while(b)
    {
        if (b->dev == dev && b->blockno == blockno)
        {
            b->refcnt++;
            release(&bcache.bucket[key].lock);
            //release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
        b=b->next;
    }
    release(&bcache.bucket[key].lock);
    // Not cached.
    // for(int i=key;i!=HASH(key-1);i=HASH(i+1)){
    //     printf("bget key:%d i:%d,0:%d\n",key,i,HASH(0));
    int i=key;
    do{
        //printf("bget key:%d i:%d,0:%d\n", key, i, HASH(0));
        acquire(&bcache.bucket[i].lock);
        b = bcache.bucket[i].head;
        while(b){

            if (b->refcnt == 0)
            {
                b->dev = dev;
                b->blockno = blockno;
                b->valid = 0;
                b->refcnt = 1;

                if(i!=key){
                    if (b != bcache.bucket[i].head){
                        prev->next = b->next;}
                    else{
                        bcache.bucket[i].head=b->next;
                    }
                    release(&bcache.bucket[i].lock);
                    acquire(&bcache.bucket[key].lock);
                    b->next = bcache.bucket[key].head;
                    bcache.bucket[key].head = b;
                    release(&bcache.bucket[key].lock);
                }else{
                    release(&bcache.bucket[i].lock);
                }
                //printf("bget: found dev=%d, blockno=%d, key =%d,i=%d\n", dev, blockno, key,i);
                //release(&bcache.lock);
                acquiresleep(&b->lock);
                return b;
            }
            prev = b;
            b=b->next;
        }
        release(&bcache.bucket[i].lock);
        i = HASH(i + 1);
    }while(i!=key);
        
    panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
    struct buf *b;

    b = bget(dev, blockno);
    if (!b->valid)
    {
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);
    int key = HASH(b->blockno);
    acquire(&bcache.bucket[key].lock);
    b->refcnt--;
    release(&bcache.bucket[key].lock);
}
void bpin(struct buf *b)
{
    int i=HASH(b->blockno);
    acquire(&(bcache.bucket[i].lock));
    b->refcnt++;
    release(&(bcache.bucket[i].lock));
}

void bunpin(struct buf *b)
{
    int i = HASH(b->blockno);
    acquire(&(bcache.bucket[i].lock));
    b->refcnt--;
    release(&(bcache.bucket[i].lock));
}