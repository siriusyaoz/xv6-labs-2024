#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};

static struct spinlock netlock;
// 定义端口绑定和数据包队列的结构
#define MAX_PENDING_PACKETS 16
#define min(a, b) ((a) < (b) ? (a) : (b))

struct pending_packet
{
  char *data;      // 数据包内容
  int len;         // 数据包长度
  uint32 src_ip;   // 源IP地址
  uint16 src_port; // 源端口
  struct pending_packet *next;
};

struct bound_port
{
  uint16 port;       // 绑定的端口号
  int pending_count; // 等待处理的包数量
  struct spinlock lock;
  struct pending_packet *head; // 包队列头
  struct pending_packet *tail; // 包队列尾
};

#define MAX_PORTS 64
struct
{
  struct spinlock lock;
  struct bound_port ports[MAX_PORTS];
  int port_count;
} udp_table;

void
netinit(void)
{
  initlock(&netlock, "netlock");
  udp_table.port_count = 0;
}

// 查找已绑定的端口
static struct bound_port *
find_bound_port(uint16 port)
{
  for (int i = 0; i < udp_table.port_count; i++)
  {
    if (udp_table.ports[i].port == port)
      return &udp_table.ports[i];
  }
  return 0;
}

//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  //
  // Your code here.
  //

  int port;
  // if (argint(0, &port) < 0)
  //   return -1;
  argint(0, &port);

  acquire(&udp_table.lock);

  // 检查端口是否已被绑定
  if (find_bound_port(port) != 0)
  {
    release(&udp_table.lock);
    return -1;
  }

  // 检查是否还有空间绑定新端口
  if (udp_table.port_count >= MAX_PORTS)
  {
    release(&udp_table.lock);
    return -1;
  }

  // 初始化新的bound_port结构
  struct bound_port *bp = &udp_table.ports[udp_table.port_count++];
  bp->port = port;
  bp->pending_count = 0;
  bp->head = bp->tail = 0;
  initlock(&bp->lock, "bound_port");

  release(&udp_table.lock);
  return 0;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //

  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  // 声明变量
  int dport;
  uint64 src_arg, sport_arg, buf;
  int maxlen;
  struct proc *p = myproc();
  struct bound_port *bp;
  struct pending_packet *pp;
  int ret = -1;

  // 获取参数，检查参数有效性
  argint(0, &dport);
  argaddr(1, &src_arg);
  argaddr(2, &sport_arg);
  argaddr(3, &buf);
  argint(4, &maxlen);

  // // 获取目标端口
  // if (fetchaddr(dport_arg, &dport_val) < 0)
  // {
  //   return -1;
  // }
  // int dport = (int)dport_val;

  // 检查端口是否已绑定
  acquire(&udp_table.lock);
  if ((bp = find_bound_port(dport)) == 0)
  {
    release(&udp_table.lock);
    return -1;
  }

  // 切换锁的持有
  acquire(&bp->lock);
  release(&udp_table.lock);

  // 等待数据包
  while (bp->head == 0)
  {
    if (p->killed)
    {
      release(&bp->lock);
      return -1;
    }
    sleep(bp, &bp->lock);
  }

  // 从队列中取出数据包
  pp = bp->head;
  bp->head = pp->next;
  if (bp->head == 0)
  {
    bp->tail = 0;
  }
  bp->pending_count--;

  // 先复制源地址和端口，因为这些比较小且固定大小
  if (copyout(p->pagetable, src_arg, (char *)&pp->src_ip, sizeof(pp->src_ip)) < 0 ||
      copyout(p->pagetable, sport_arg, (char *)&pp->src_port, sizeof(pp->src_port)) < 0)
  {
    goto bad;
  }

  // 复制数据，注意长度限制
  int copy_len = pp->len;
  if (copy_len > maxlen)
    copy_len = maxlen;

  if (copyout(p->pagetable, buf, pp->data, copy_len) < 0)
  {
    goto bad;
  }

  // 设置返回值为实际拷贝的数据长度
  ret = copy_len;

bad:
  // 清理资源
  if (pp->data)
    kfree(pp->data);
  kfree(pp);
  release(&bp->lock);
  return ret;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)
  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1)
  {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  // 基本长度检查
  // if (len < sizeof(struct eth) + sizeof(struct ip)){
  //   printf("not ip length!\n");
  //   return;}

  struct eth *ethhdr = (struct eth *)buf;
  struct ip *iphdr = (struct ip *)(ethhdr + 1);

  // IP头部检查
  // if (iphdr->ip_p != IPPROTO_UDP){
  //   printf("not ip header!\n");
  //   return;}

  // UDP头部长度检查
  // if (len < sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp)){
  //   printf("not udp length\n"); 
  //   return;}

  struct udp *udphdr = (struct udp *)(iphdr + 1);
  int udp_len = ntohs(udphdr->ulen);
  if (udp_len < sizeof(struct udp) ||
      len < sizeof(struct eth) + sizeof(struct ip) + udp_len)
    return;

  uint16 dport = ntohs(udphdr->dport);

  // 查找绑定的端口
  acquire(&udp_table.lock);
  struct bound_port *bp = find_bound_port(dport);
  if (bp == 0)
  {
    printf("bp is 0!\n");
    kfree(buf);
    release(&udp_table.lock);
    return;
  }

  // 切换锁
  acquire(&bp->lock);
  release(&udp_table.lock);

  // 检查队列容量
  if (bp->pending_count >= MAX_PENDING_PACKETS)
  {
    printf("pending counts is larger than 16!\n");
    kfree(buf);
    release(&bp->lock);
    return;
  }

  // 分配内存
  int payload_len = ntohs(udphdr->ulen) - sizeof(struct udp);
  char *data = kalloc();
  if (data == 0)
  {
    printf("kalloc data failed\n");
    kfree(buf);
    release(&bp->lock);
    return;
  }

  struct pending_packet *pp = kalloc();
  if (pp == 0)
  {
    printf("kalloc pending packet failed\n");
    kfree(data);
    kfree(buf);
    release(&bp->lock);
    return;
  }

  // 初始化新包
  memset(pp, 0, sizeof(*pp));
  memmove(data, (char *)(udphdr + 1), payload_len);
  pp->data = data;
  pp->len = payload_len;
  pp->src_ip = ntohl(iphdr->ip_src);
  pp->src_port = ntohs(udphdr->sport);
  pp->next = 0;

  // 更新队列
  if (bp->tail == 0)
  {
    bp->head = bp->tail = pp;
  }
  else
  {
    bp->tail->next = pp;
    bp->tail = pp;
  }
  bp->pending_count++;
  
  // 唤醒等待的进程
  wakeup(bp);
  kfree(buf);
  release(&bp->lock);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
