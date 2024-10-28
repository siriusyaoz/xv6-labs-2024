### recv的实现逻辑
e1000收到数据后，引起中断，在trap.c中有

```c
#ifdef LAB_NET
    else if(irq == E1000_IRQ){
      e1000_intr();
    }
#endif
```
在e1000_intr中调用e1000_recv函数。而e1000_recv的作用是
Check for packets that have arrived from the e1000
Create and deliver a buf for each packet (using net_rx()).

e1000_recv的实现中：
1. 使用while(1)循环，一次处理一个packet直到缓冲区没有数据
2. 要注意再次kalloc rx_buf[tail],可以看见arp_rx中最后的kfree操作。
3.  // 接收环形缓冲区(RX Ring) :
    // RDT(Receive Descriptor Tail)：软件维护，指向最后一个被处理的描述符
    // RDH(Receive Descriptor Head)：由硬件维护，指向下一个将被写入的位置

```c
// Deliver packet to network stack
      net_rx(rx_bufs[tail], length);
```
在net_rx函数中判断是arp还是ip包。若是ip包，调用ip_rx函数。
目前只实现了UDP包的接收。

```c
struct pending_packet
struct bound_port
struct udp_table;
```
ip_rx函数在udp_table中维护通过收到的所有bind后的port数组，每个port中又有很多packet，以链表的形式相连。
如果kalloc失败或者收到的buf(e_1000收到的数据)对应的port未bind,或者超过maxpackets,直接丢弃该包，这时要注意kfree(buf)以免内存泄漏。

  
ip_rx函数在将buf存到udp_table之后就` wakeup(bp);`// 唤醒等待的sys_recv进程 .
sys_recv是对标准的recv() sockets api的实现。
使用copyout函数将udp包中的data, IP source address, UDP source port从内核中搬运到用户空间中去。
最后不论是否出错，都要ip_rx中kalloc的数据结构在这一步kfree掉(goto bad)。


注意多次出现的，在有两把锁时，**切换锁的持有** 这一操作
