### kalloctest
#### 实现
对每个CPU都建一个freelist：
```c
struct {
  struct spinlock lock;
  struct run *freelist;
  char name[8];
} kmem[NCPU];
```
这里的name是为了存每个freelist的名字。  

kfree的改动很简单，只要在free时还到所在的CPU的freelist中去：
```c
push_off();
int cpu_id = cpuid();
acquire(&kmem[cpu_id].lock);
r->next = kmem[cpu_id].freelist;
kmem[cpu_id].freelist = r;
release(&kmem[cpu_id].lock);
pop_off();
```
kalloc的实现：  
如该cpu(记为cpuid )的freelist中还有空余的page,将它分配给调用者。    
否则，从下一个cpu的id开始，循环遍历。在freelist非空的cpuid（设为cpuid i)中偷取一定数量的page。分配给调用者。同时更新cpuid和i的freelist信息。    
这里的关键在于偷取策略的选取：  
1. 只偷取一个
2. 偷取一个BATCH_SIZE (3或4)
3. 偷取freelist的一半 

3个策略都有实现，可惜都未达到10000个test_and_set的目标.  
另外还要注意的是lock的acquire和release的时间点。


#### 时间
start
2024/11/1 16:00-24:00
2024/11/2 7:00-8:48
kalloctest passed!
这里的玄学太多了！一句话的增删对性能的影响太大了！
在make grade里再试一次，又失败了！
暂时跳过吧？看看之后会不会降低难度？
但是教授确实实现了？他们是如何做到的？

#### 测试结果
kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem0: #test-and-set 0 #acquire() 582128 
lock: kmem1: #test-and-set 0 #acquire() 520186  
lock: kmem2: #test-and-set 0 #acquire() 415519  
lock: kmem3: #test-and-set 0 #acquire() 29912  
lock: kmem4: #test-and-set 0 #acquire() 29912  
lock: kmem5: #test-and-set 0 #acquire() 29912  
lock: kmem6: #test-and-set 0 #acquire() 29912  
lock: kmem7: #test-and-set 0 #acquire() 29912  
lock: bcache_bucket0: #test-and-set 0 #acquire() 12493  
lock: bcache_bucket1: #test-and-set 0 #acquire() 12201  
lock: bcache_bucket2: #test-and-set 0 #acquire() 24058  
lock: bcache_bucket3: #test-and-set 0 #acquire() 23940  
lock: bcache_bucket4: #test-and-set 0 #acquire() 23470
lock: bcache_bucket5: #test-and-set 0 #acquire() 26402
lock: bcache_bucket6: #test-and-set 112 #acquire() 44865
lock: bcache_bucket7: #test-and-set 78 #acquire() 593153
lock: bcache_bucket8: #test-and-set 0 #acquire() 56925
lock: bcache_bucket9: #test-and-set 0 #acquire() 57798
lock: bcache_bucket10: #test-and-set 0 #acquire() 16107
lock: bcache_bucket11: #test-and-set 0 #acquire() 12780
lock: bcache_bucket12: #test-and-set 0 #acquire() 10395
--- top 5 contended locks:
lock: proc: #test-and-set 32648750 #acquire() 2909650
lock: wait_lock: #test-and-set 25059931 #acquire() 23658
lock: proc: #test-and-set 24696042 #acquire() 1524023
lock: virtio_disk: #test-and-set 17504459 #acquire() 131780
lock: proc: #test-and-set 10202872 #acquire() 949477
tot= 190
test1 OK
start test2
total free number of pages: 32464 (out of 32768)
.....  
test2 OK  
start test3  
..........child done 100000
--- lock kmem/bcache stats
lock: kmem0: #test-and-set 8197 #acquire() 2115985  
lock: kmem1: #test-and-set 916 #acquire() 2054093
lock: kmem2: #test-and-set 108 #acquire() 1725115
lock: kmem3: #test-and-set 0 #acquire() 168125
lock: kmem4: #test-and-set 0 #acquire() 168125
lock: kmem5: #test-and-set 0 #acquire() 168125
lock: kmem6: #test-and-set 0 #acquire() 168125
lock: kmem7: #test-and-set 0 #acquire() 168125
lock: bcache_bucket0: #test-and-set 0 #acquire() 12493
lock: bcache_bucket1: #test-and-set 0 #acquire() 12201
lock: bcache_bucket2: #test-and-set 0 #acquire() 24058
lock: bcache_bucket3: #test-and-set 0 #acquire() 23940
lock: bcache_bucket4: #test-and-set 0 #acquire() 23470
lock: bcache_bucket5: #test-and-set 0 #acquire() 26402
lock: bcache_bucket6: #test-and-set 112 #acquire() 44865
lock: bcache_bucket7: #test-and-set 78 #acquire() 593253
lock: bcache_bucket8: #test-and-set 0 #acquire() 56925
lock: bcache_bucket9: #test-and-set 0 #acquire() 57798
lock: bcache_bucket10: #test-and-set 0 #acquire() 16107
lock: bcache_bucket11: #test-and-set 0 #acquire() 12780
lock: bcache_bucket12: #test-and-set 0 #acquire() 10395  
--- top 5 contended locks:  
lock: proc: #test-and-set 32649017 #acquire() 3174092
lock: wait_lock: #test-and-set 25059931 #acquire() 23663
lock: proc: #test-and-set 24696200 #acquire() 1929429
lock: virtio_disk: #test-and-set 17504459 #acquire() 131780
lock: proc: #test-and-set 10202878 #acquire() 954822  
tot= 9411  
  
test3 OK
偶尔work???

### bcachetest
#### 实现
在我原来的实现中，当refcnt减为0时，就将它从bucket中删除，从而漂移在所有的buckets之外。但是：  
来自claude sonnet3.5(new)
>我们在 brelse 中移除块时，没有清除它的其他信息(dev, blockno等)。
然后在 bget 中，我们试图通过遍历链表找到这个块，但因为它已经被移除出链表了，所以找不到，
就会去寻找新的空闲块。  
修改建议：
在 brelse 中，当 refcnt 变为0时不要从链表中移除块
这样，当一个块的 refcnt 变为0时，它仍然保持在链表中，下次 bget 就能找到它。这样可以：
保持缓存的局部性
减少不必要的链表操作
确保块的信息(dev, blockno等)与其在链表中的位置保持一致

思路清晰后就很简单了。  
binit:    
首先将所有的buf都分配给bucket[0]。 

bget:   
如果dev 和blockno 已经在某个buf中了，直接refcnt++。 
如果在当前bucket中找到了refcnt==0的buf,直接将它分配给调用者。   
否则循环到下一个bucket，在其中寻找refcnt==0的buf。
同样找注意这个过程中两个lock的acquire和release。    


2024 11/3 11:00完成